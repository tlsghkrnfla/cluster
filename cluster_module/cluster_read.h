#ifndef __CLUSTER_READ_H
#define __CLUSTER_READ_H
#include <linux/pagemap.h>

int cluster_find_pbn(struct inode *, pgoff_t, unsigned long *);
struct page *cluster_find_get_page(struct address_space *, pgoff_t, void **, void ***, unsigned long *);
ssize_t cluster_do_generic_file_read(struct file *, loff_t *, struct iov_iter *, ssize_t);

#endif

