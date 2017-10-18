#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/smp.h>
#include "nvme.h"

struct nvme_dev *get_nvme_dev(struct inode *inode)
{
	return (struct nvme_dev *)((struct nvme_ns *)(inode->i_sb->s_bdev->bd_disk->private_data))->dev;
}

struct nvme_queue *nvme_polling_qpair(struct inode *inode)
{
	struct nvme_dev *dev = get_nvme_dev(inode);

	return dev->queues[smp_processor_id() + 5];
}

struct nvme_queue *nvme_interrupt_qpair(struct inode *inode)
{
	struct nvme_dev *dev = get_nvme_dev(inode);

	//return dev->queues[4];
	return dev->queues[smp_processor_id() + 1];
}

void nvme_put_interrupt_qpair(void)
{
	put_cpu();
}
