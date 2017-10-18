#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#include "nvme.h"

void nvme_create_request_pool(struct nvme_queue *nvmeq)
{
	nvmeq->kmem_reqpool = kmem_cache_create("req_pool", sizeof(struct nvme_request),
									0, SLAB_HWCACHE_ALIGN, NULL);
	nvmeq->mem_reqpool = mempool_create(512, mempool_alloc_slab, mempool_free_slab,
							nvmeq->kmem_reqpool);
}

void nvme_destroy_request_pool(struct nvme_queue *nvmeq)
{
	mempool_destroy(nvmeq->mem_reqpool);
	kmem_cache_destroy(nvmeq->kmem_reqpool);
}

struct nvme_request *nvme_alloc_request(struct nvme_queue *nvmeq)
{
	struct nvme_request *req = mempool_alloc(nvmeq->mem_reqpool, GFP_KERNEL);
	memset(req, 0, sizeof(*req));

	return req;
}

void nvme_free_request(struct nvme_request *req, struct nvme_queue *nvmeq)
{
	mempool_free(req, nvmeq->mem_reqpool);
}

