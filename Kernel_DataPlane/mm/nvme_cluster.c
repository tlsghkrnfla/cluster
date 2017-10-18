
/*
 * Copyright (C) 2016 byeonghun hyeon
 * This program is free software;
 */

#include <linux/kernel.h>
#include <linux/nvme_cluster.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/mm_types.h>

struct NVME_cluster nvme_cluster;
struct NVME_cluster *NVME = &nvme_cluster;

struct NVME_cluster* get_NVME_cluster(void)
{
	return NVME;
}
EXPORT_SYMBOL(get_NVME_cluster);

int NVME_build_page_lbn_table(struct address_space *mapping)
{
	if ((NVME->enable) && (NVME->NVME_ops) && (NVME->NVME_ops->build_page_lbn_table))
		return NVME->NVME_ops->build_page_lbn_table(mapping);
	else
		return 0;
}
EXPORT_SYMBOL(NVME_build_page_lbn_table);

unsigned int NVME_get_delay_flag(void)
{
	if ((NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->get_delay_flag))
		return NVME->NVME_flag->get_delay_flag();
	else 
		return 0;
}
EXPORT_SYMBOL(NVME_get_delay_flag);

unsigned int NVME_get_hole_flag(void)
{
	if ((NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->get_hole_flag))
		return NVME->NVME_flag->get_hole_flag();
	else 
		return 0;
}
EXPORT_SYMBOL(NVME_get_hole_flag);

void NVME_set_delay_flag(unsigned int *flag)
{
	if ((NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->set_delay_flag))
		NVME->NVME_flag->set_delay_flag(flag);
}
EXPORT_SYMBOL(NVME_set_delay_flag);

void NVME_set_hole_flag(unsigned int *flag)
{
	if ((NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->set_hole_flag))
		NVME->NVME_flag->set_hole_flag(flag);
}
EXPORT_SYMBOL(NVME_set_hole_flag);

unsigned int NVME_check_hole_flag(unsigned int flag)
{
	if( (NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->check_hole_flag))
		return NVME->NVME_flag->check_hole_flag(flag);
	else
		return 0;
}
EXPORT_SYMBOL(NVME_check_hole_flag);

unsigned int NVME_check_delay_flag(unsigned int flag)
{
	if ((NVME->enable) && (NVME->NVME_flag) && (NVME->NVME_flag->check_delay_flag))
		return NVME->NVME_flag->check_hole_flag(flag);
	else
		return 0;
}
EXPORT_SYMBOL(NVME_check_delay_flag);

struct page* NVME_find_get_page(struct address_space *mapping, unsigned int index,
							void **node, void ***slot)
{
	if ((NVME->enable) && (NVME->NVME_ops) && (NVME->NVME_ops->find_get_page))
		return NVME->NVME_ops->find_get_page(mapping, index, node, slot);
	return (struct page *)NOTHING;
}
EXPORT_SYMBOL(NVME_find_get_page);

int NVME_read_page(void *node, unsigned int index, struct page* page)
{
	if ((NVME->enable) && (NVME->NVME_ops) && (NVME->NVME_ops->read_page))
		return NVME->NVME_ops->read_page(node, index, page);
	return NOTHING;
}
EXPORT_SYMBOL(NVME_read_page);

ssize_t NVME_do_generic_file_read(struct file *filp, loff_t *ppos,
			struct iov_iter *iter, ssize_t written)
{
	if ((NVME->enable) && (NVME->NVME_ops) && (NVME->NVME_ops->do_generic_file_read))
		return NVME->NVME_ops->do_generic_file_read(filp, ppos, iter, written);
	return NOTHING;
}
EXPORT_SYMBOL(NVME_do_generic_file_read);
