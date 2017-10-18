#include "cluster_nvme_lib.h"

int cluster_nvme_read(struct inode* inode, struct nvme_queue *qpair,
			struct page*page, uint64_t end_io) {

}

struct nvme_queue* cluster_get_nvme_qpair_polling(void){
	return nvme_get_polling_qpair(void); 
}

struct nvme_queue* cluster_get_nvme_qpair_interrupt(void){
	return nvme_get_interrupt_qpair(void);
}


void cluster_build_nvme_mapping_table(struct nvme_dev* dev) {
	struct cluster_info *info = dev->c_info;
	info->polling_map = NVME_make_queue_map(info->num_poll, info->node);
	info->interrupt_map = NVME_make_queue_map(info->num_intr, info->node);}
