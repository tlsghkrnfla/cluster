#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/nvme_cluster.h>
#include "nvme.h"
#include "nvme_request.h"
#include "nvme_get_qpair.h"
#include <trace/events/latency.h>

int cluster_pre_dma_mapping(struct notifier_block *self, unsigned long val, void *_data)
{
	struct notifier_data *data = (struct notifier_data *)_data;
	struct nvme_dev *dev = get_nvme_dev(data->mapping->host);
	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *dma_unmap_list = per_cpu_ptr(cluster_nvme->dma_unmap_list,
															smp_processor_id());
	nvme_pre_dma_mapping(dev, &data->pagelist, data->total_req_size);
	nvme_post_dma_unmapping(dev, dma_unmap_list);

	return 0;
}

int cluster_nvme_polling(struct notifier_block *self, unsigned long val, void *_data)
{
	struct notifier_data *data = (struct notifier_data *)_data;
	struct nvme_queue *qpair = nvme_polling_qpair(data->mapping->host);

	while (1) {
		if ((le16_to_cpu(qpair->cqes[qpair->cq_head].status) & 1) == qpair->cq_phase) {
			nvme_process_cq(qpair);
			//break;
		}
		if (!data->total_req_num)
			break;
	}

	return 0;
}

struct notifier_block cluster_pre_dma_mapping_chain = {
	.notifier_call = cluster_pre_dma_mapping,
	.priority = 0,
};
struct notifier_block cluster_nvme_polling_chain = {
	.notifier_call = cluster_nvme_polling,
	.priority = 0,
};

static void nvme_submit_io(struct nvme_queue *io_q, struct nvme_request *req)
{
	struct nvme_command *cmd = &req->cmd;
	u16 tail;

	tail = io_q->sq_tail;
	memcpy(&io_q->sq_cmds[tail], cmd, sizeof(*cmd));
	if (++tail == io_q->q_depth) {
		tail = 0;
	}

	writel(tail, io_q->q_db);
	//cluster_rdtsc(NVME_latency.lo, NVME_latency.hi, &NVME_latency.b);
	io_q->sq_tail = tail;
}

struct nvme_request *nvme_init_request(struct nvme_queue *qpair, struct page *page, int req_size)
{
	struct nvme_request *req = nvme_alloc_request(qpair);

	req->data = &current->notifier_data;
	req->iod = ___nvme_alloc_iod(req_size, req_size * PAGE_SIZE, qpair->dev, 0, GFP_ATOMIC);
	sg_init_table(req->iod->sg, req_size);
	req->cmd.rw.slba = cpu_to_le64((page->private + 256) * 8);

	return req;
}

void nvme_set_request(struct nvme_queue *qpair, struct nvme_request *req, int size)
{
	struct nvme_iod *iod = req->iod;

	__nvme_setup_prps(qpair->dev, iod, size * PAGE_SIZE, GFP_ATOMIC);
	req->size = size;
	req->iod->nents = size;
	req->cmd.rw.prp1 = cpu_to_le64(sg_dma_address(iod->sg));
	req->cmd.rw.prp2 = cpu_to_le64(iod->first_dma);
	nvme_make_command(qpair, req, size);
	qpair->req_tags[qpair->sq_tail] = req;
}

void nvme_set_async_request(struct nvme_queue *qpair, struct nvme_request *req, int size)
{
	struct nvme_iod *iod = req->iod;

	__nvme_setup_prps(qpair->dev, iod, size * PAGE_SIZE, GFP_ATOMIC);
	req->size = size;
	req->iod->nents = size;
	req->cmd.rw.prp1 = cpu_to_le64(sg_dma_address(iod->sg));
	req->cmd.rw.prp2 = cpu_to_le64(iod->first_dma);
	nvme_make_async_command(qpair, req, size);
	qpair->req_tags[qpair->sq_tail] = req;
}

void nvme_set_sg(struct scatterlist **sg, struct nvme_request *req, struct page *page)
{
	if (!(*sg))
		*sg = req->iod->sg;
	else {
		sg_unmark_end(*sg);
		*sg = sg_next(*sg);
	}
	sg_set_page(*sg, page, PAGE_SIZE, 0);
	(*sg)->dma_address = ((struct scatterlist *)page->mapping)->dma_address;
	(*sg)->dma_length = ((struct scatterlist *)page->mapping)->dma_length;
}

int nvme_direct_readpages(struct inode *inode, unsigned int req_size)
{
	struct notifier_data *data = &current->notifier_data;
	struct nvme_queue *qpair = nvme_polling_qpair(inode);
	struct nvme_request *req = NULL;
	struct scatterlist *sg = NULL;
	struct list_head *p;
	int size = 0;

	list_for_each_prev(p, &data->pagelist) {
		struct page *page = list_entry(p, struct page, lru);
		if (page->private) {
			if (req) {
				sg_mark_end(sg);
				nvme_set_request(qpair, req, size);
				nvme_submit_io(qpair, req);
			}
			req = nvme_init_request(qpair, page, req_size);
			req->end_io = data->end_io;
			data->total_req_num++;	
			sg = NULL;
			size = 0;
		}
		nvme_set_sg(&sg, req, page);
/*
		if (!sg)
			sg = req->iod->sg;
		else {
			sg_unmark_end(sg);
			sg = sg_next(sg);
		}
		sg_set_page(sg, page, PAGE_SIZE, 0);
		sg->dma_address = ((struct scatterlist *)page->mapping)->dma_address;
		sg->dma_length = ((struct scatterlist *)page->mapping)->dma_length;
*/

		req->req_page[size] = page;
		size++;
	}
	sg_mark_end(sg);
	nvme_set_request(qpair, req, size);
	nvme_submit_io(qpair, req);

	atomic_notifier_chain_register(current->device_driver_chain, &cluster_pre_dma_mapping_chain);
	atomic_notifier_chain_register(current->polling_chain, &cluster_nvme_polling_chain);

	return 0;
}

int nvme_direct_async_readpages(struct inode *inode, unsigned int req_size)
{
	struct notifier_data *data = &current->notifier_data;
	struct nvme_queue *qpair = nvme_interrupt_qpair(inode);
	struct nvme_request *req = NULL;
	struct scatterlist *sg = NULL;
	struct list_head *p;
	int size = 0;

	//spin_lock_irq(&qpair->q_lock);
	list_for_each_prev(p, &data->pagelist) {
		struct page *page = list_entry(p, struct page, lru);
		if (page->private) {
			if (req) {
				sg_mark_end(sg);
				spin_lock_irq(&qpair->q_lock);
				nvme_set_async_request(qpair, req, size);
				nvme_submit_io(qpair, req);
				spin_unlock_irq(&qpair->q_lock);
			}
			req = nvme_init_request(qpair, page, req_size);
			req->end_io = data->end_io;
			//data->total_req_num++;	
			sg = NULL;
			size = 0;
		}

		nvme_set_sg(&sg, req, page);
/*
		if (!sg)
			sg = req->iod->sg;
		else {
			sg_unmark_end(sg);
			sg = sg_next(sg);
		}
		sg_set_page(sg, page, PAGE_SIZE, 0);
		sg->dma_address = ((struct scatterlist *)page->mapping)->dma_address;
		sg->dma_length = ((struct scatterlist *)page->mapping)->dma_length;
*/

		req->req_page[size] = page;
		size++;
	}
	sg_mark_end(sg);
	spin_lock_irq(&qpair->q_lock);
	nvme_set_async_request(qpair, req, size);
	nvme_submit_io(qpair, req);
	spin_unlock_irq(&qpair->q_lock);

	atomic_notifier_chain_register(current->device_driver_chain, &cluster_pre_dma_mapping_chain);

	return 0;
}

void nvme_direct_write(struct inode *inode, struct page *page, unsigned long pbn) {
	return;
}

//cluster_rdtsc(NVME_latency.lo, NVME_latency.hi, &NVME_latency.chain_end);

