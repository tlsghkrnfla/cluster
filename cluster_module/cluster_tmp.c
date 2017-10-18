#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include "nvme.h"
void print_name(struct inode *inode)
{
	struct nvme_dev *dev =((struct nvme_ns *)(inode->i_sb->s_bdev->bd_disk->private_data))->dev;
	printk("Device name is %s\n", dev->name);
}
