#ifndef __CLUSTER_OPEN_H
#define __CLUSTER_OPEN_H
#include <linux/fs.h>
int cluster_build_page_lbn_table(struct address_space *);
int cluster_add_page_lbn_table(struct address_space *, pgoff_t);
void cluster_pagepool_dealloc(struct address_space *);
#endif
