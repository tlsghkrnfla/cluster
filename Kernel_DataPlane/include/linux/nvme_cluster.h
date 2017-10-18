#ifndef _LINUX_NVME_CLUSTER_H
#define _LINUX_NVME_CLUSTER_H

#define NOTHING				-100000
#define EXT4_MAP_DELAYED	(1 << 1)
#define EXT4_MAP_HOLE		(1 << 2)
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm_types.h>

/*------------------------ data structure ---------------------------------------*/
struct NVME_flag_operations {
	unsigned int (*get_delay_flag)(void);
	unsigned int (*get_hole_flag)(void);
	void (*set_delay_flag)(unsigned int *);
	void (*set_hole_flag)(unsigned int *);
	unsigned int (*check_delay_flag)(unsigned int);
	unsigned int (*check_hole_flag)(unsigned int);
};

struct NVME_page_lbn_operations {
	int (*build_page_lbn_table)(struct address_space *);
	struct page *(*find_get_page)(struct address_space *, unsigned int, void **, void ***);
	int (*read_page)(void *, unsigned int, struct page *);
	ssize_t (*do_generic_file_read)(struct file *filp, loff_t *ppos,
							struct iov_iter *iter, ssize_t written); 
};

struct NVME_cluster
{
	int enable;
	struct NVME_flag_operations *NVME_flag;
	struct NVME_page_lbn_operations *NVME_ops;

	struct list_head *pagelist;
	struct list_head *dma_unmap_list;

};


/*------------------------ module intervlace --------------------------------------*/


//flag interface
unsigned int NVME_get_delay_flag(void);
unsigned int NVME_get_hole_flag(void);
void NVME_set_delay_flag(unsigned int * flag);
void NVME_set_hole_flag(unsigned int * flag);
unsigned int NVME_check_hole_flag(unsigned int flag);
unsigned int NVME_check_delay_flag(unsigned int flag);



//page_lbn_interface
int NVME_build_page_lbn_table(struct address_space *mapping);
int NVME_get_pbn(struct file* file);




//NVME_cluster interface
struct NVME_cluster* get_NVME_cluster(void);


struct page* NVME_find_get_page(struct address_space *, unsigned int, void **, void ***);
int NVME_read_page(void *, unsigned int, struct page *);


ssize_t NVME_do_generic_file_read(struct file *,loff_t *, struct iov_iter *, ssize_t);

#endif /* _LINUX_NVME_CLUSTER_H */
