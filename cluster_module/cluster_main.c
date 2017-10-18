#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/nvme_cluster.h>
#include "cluster_flag.h"
#include "cluster_open.h"
#include "cluster_read.h"
#include "cluster_debug.h"
#include "nvme.h"
#include "cluster_readahead.h"

static struct NVME_flag_operations cluster_flag = {
	.get_delay_flag		= cluster_get_delay_flag,
	.get_hole_flag		= cluster_get_hole_flag,
	.set_delay_flag		= cluster_set_delay_flag,
	.set_hole_flag		= cluster_set_hole_flag,
	.check_delay_flag	= cluster_check_delay_flag,
	.check_hole_flag	= cluster_check_hole_flag
};

static struct NVME_page_lbn_operations cluster_ops = {
	.build_page_lbn_table			= cluster_build_page_lbn_table,
	.do_generic_file_read			= cluster_do_generic_file_read,
	//.do_page_cache_sync_readahead	= cluster_page_cache_sync_readahead,
	//.do_page_cache_readahead		= cluster_do_page_cache_readahead,
	//.do_page_cache_read		= cluster_readpage
};

/* Allocation of per-CPU pagepool */
static void NVME_pagepool_alloc(void)
{
	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *pagelist;
	struct page *page;
	int i, cpu_num, page_num = 256;

	cluster_nvme->pagelist = alloc_percpu(struct list_head);
	cluster_nvme->dma_unmap_list = alloc_percpu(struct list_head);
	/* alloc pages to each per-CPU pagepool */
	for_each_possible_cpu(cpu_num) {
		if (!cpumask_test_cpu(cpu_num, cpu_online_mask))
			continue;
		pagelist = per_cpu_ptr(cluster_nvme->pagelist, cpu_num);
		INIT_LIST_HEAD(pagelist);
		/* Max request size 128K */
		for (i = 0; i < page_num; i++) {
			page = alloc_pages(GFP_HIGHUSER_MOVABLE|__GFP_COLD|__GFP_NORETRY|__GFP_NOWARN, 0);
			if (page == NULL) printk("Alloc page ERROR!!\n");
			list_add(&page->lru, pagelist);
		}
		pagelist = per_cpu_ptr(cluster_nvme->dma_unmap_list, cpu_num);
		INIT_LIST_HEAD(pagelist);
	}

	return;
}

/* Deallocation of per-CPU pagepool */
static void NVME_pagepool_dealloc(void)
{
	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *pagelist;
	struct page *page;
	int cpu_num;

	/* dealloc pages in each per-CPU pagepool */
	for_each_possible_cpu(cpu_num) {
		if (!cpumask_test_cpu(cpu_num, cpu_online_mask))
			continue;
		pagelist = per_cpu_ptr(cluster_nvme->pagelist, cpu_num);
		while (!list_empty(pagelist)) {
			page = list_last_entry(pagelist, struct page, lru);
			list_del(&page->lru);
			__free_pages(page, 0);
		}
	}
	free_percpu(cluster_nvme->pagelist);
	free_percpu(cluster_nvme->dma_unmap_list);

	return;
}

int init_cluster_main(void)
{
	struct NVME_cluster *cluster_nvme;

	DEBUG_ENTRY;
	cluster_nvme = get_NVME_cluster();
	if (!cluster_nvme) {
		DEBUG_PRINT(ERROR, "install NVMe cluster\n");
		return 0;
	}
	NVME_pagepool_alloc();
	cluster_nvme->NVME_flag = &cluster_flag;
	cluster_nvme->NVME_ops = &cluster_ops;
	cluster_nvme->enable = 1;
	DEBUG_EXIT;

	return 0;
}

void exit_cluster_main(void)
{
	struct NVME_cluster *cluster_nvme;

	DEBUG_ENTRY;
	cluster_nvme = get_NVME_cluster();
	if (cluster_nvme->pagelist)
		NVME_pagepool_dealloc();
	cluster_nvme->NVME_flag = NULL;
	cluster_nvme->NVME_ops = NULL;
	cluster_nvme->enable = 0;
	DEBUG_EXIT;

	return;
}

