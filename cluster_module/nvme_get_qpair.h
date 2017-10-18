#ifndef __NVME_GET_QPAIR_H
#define __NVME_GET_QPAIR_H
#include <linux/fs.h>
#include "nvme.h"

struct nvme_dev *get_nvme_dev(struct inode *);
struct nvme_queue *nvme_interrupt_qpair(struct inode *);
struct nvme_queue *nvme_polling_qpair(struct inode *);
void nvme_put_interrupt_qpair(void);

#endif
