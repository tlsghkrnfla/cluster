#ifndef __NVME_REQUEST_H
#define __NVME_REQUEST_H
#include "nvme.h"


void nvme_create_request_pool(struct nvme_queue *nvmeq);
void nvme_destroy_request_pool(struct nvme_queue *nvmeq);
struct nvme_request* nvme_alloc_request(struct nvme_queue *nvmeq);
void nvme_free_request(struct nvme_request *req, struct nvme_queue *nvmeq);
#endif
