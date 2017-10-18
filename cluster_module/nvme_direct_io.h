#ifndef __NVME_DIRECT_IO_H
#define __NVME_DIRECT_IO_H

#include <linux/pagemap.h>
#include <linux/fs.h>
#include "nvme.h"

void nvme_direct_readpages(struct inode *, unsigned int);
void nvme_direct_async_readpages(struct inode *, unsigned int);
void nvme_direct_write(struct inode *, struct page *, unsigned long);
void nvme_polling(void);

#endif
