#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/backing-dev.h>
#include <linux/export.h>
#include <linux/pagevec.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/swap.h>
#include <linux/gfp.h>
#include <linux/nvme_cluster.h>
#include "cluster_open.h"
#include "cluster_read.h"
#include "cluster_flag.h"
#include "cluster_debug.h"
#include "cluster_tmp.h"
#include "nvme_direct_io.h"
#include "cluster_readahead.h"
#include <trace/events/latency.h>

void cluster_readpages(struct address_space *, unsigned int);
void cluster_async_readpages(struct address_space *, unsigned int);
struct page *cluster_do_page_cache_async_readahead(struct address_space *, struct file *,
												pgoff_t, unsigned long, unsigned long);

int cluster_mpage_end_io(struct page **req_page, int size, int error)
{
	struct page *page;
	int count;

	for (count = 0; count < size; count++) {
		page = req_page[count];

		if (!error) {
			ClearPageError(page);
			SetPageUptodate(page);
		} else {
			ClearPageUptodate(page);
			SetPageError(page);
		}
		if (!count) {
			unlock_page(page);
			continue;
		}
		unlock_page(page);
		page_cache_release(page);
	}

	return 0;
}

int cluster_mpage_async_end_io(struct page **req_page, int size, int error)
{
	struct page *page;
	int count;

	for (count = 0; count < size; count++) {
		page = req_page[count];

		while (!oncache_page(page));
		__clear_page_oncache(page);
		if (!error) {
			ClearPageError(page);
			SetPageUptodate(page);
		} else {
			ClearPageUptodate(page);
			SetPageError(page);
		}
		unlock_page(page);
		page_cache_release(page);
	}

	return 0;
}

int cluster_pagecache_syncio_processing(struct notifier_block *self, unsigned long val, void *_data)
{
	struct notifier_data *data = (struct notifier_data *)_data;
	struct address_space *mapping = data->mapping;
	struct page *page;
	int error, count = 0;
	int prev_node_index = -1, node_index = 0;

	while (!list_empty(&data->pagelist)) {
		struct radix_tree_node *node;
		void **slot;
		pgoff_t index;
		unsigned long pbn = 0;
		unsigned long pbn_offset;

		page = list_last_entry(&data->pagelist, struct page, lru);
		list_del(&page->lru);
		index = page->index;
		pbn_offset = index & RADIX_TREE_MAP_MASK;
		page->private = 0;
		kfree(page->mapping);
		/* Page cache update */
		error = add_to_page_cache_lru(page, mapping, index,
							mapping_gfp_constraint(mapping, GFP_KERNEL));
		if (error) {
			if (error == -EEXIST)
				printk("duplicated I/O request on same index\n");
			else {
				printk("after tree insert terror!!\n");
				page_cache_release(page);
			}
		}
		/* PBN caching */
		node_index = index / 64;
		if (prev_node_index != node_index) {
			rcu_read_lock();
			radix_tree_lookup_all(&mapping->page_tree, index, &node, &slot);
			rcu_read_unlock();
		}
		pbn = node->pbn[pbn_offset];
		if (pbn) {
			clear_bit(PBN_DELAY_CACHE_BIT, (void *)(&(node->pbn[pbn_offset])));
			if (PBN_is_delay_cache(node->pbn[pbn_offset]))
				printk("Release delay tree insert error!!\n");
		} else
			cluster_add_page_lbn_table(mapping, index);
	}
	/* Page re-allocation */
	for (count = 0; count < data->total_req_size; count++) {
		page = page_cache_alloc_readahead(mapping);
		if (!page) printk("Page reallocation error!!\n");
		list_add(&page->lru, &data->pagelist);
	}

	return 0;
}

int cluster_pagecache_asyncio_processing(struct notifier_block *self, unsigned long val, void *_data)
{
	struct notifier_data *data = (struct notifier_data *)_data;
	struct address_space *mapping = data->mapping;
	struct page *page;
	int error, count = 0;
	int prev_node_index = -1, node_index = 0;

	while (!list_empty(&data->pagelist)) {
		struct radix_tree_node *node = NULL;
		void **slot;
		pgoff_t index;
		unsigned long pbn = 0;
		unsigned long pbn_offset;

		page = list_last_entry(&data->pagelist, struct page, lru);
		list_del(&page->lru);
		index = page->index;
		pbn_offset = index & RADIX_TREE_MAP_MASK;
		page->private = 0;
		kfree(page->mapping);
		/* Page add to lru list */
		error = add_to_page_cache_lru(page, mapping, index,
							mapping_gfp_constraint(mapping, GFP_KERNEL));
		if (error) {
			if (error == -EEXIST)
				printk("duplicated I/O request on same index\n");
			else {
				printk("after tree insert terror!!\n");
				page_cache_release(page);
			}
		}
		__set_page_oncache(page);
		/* PBN caching */
		node_index = index / 64;
		if (prev_node_index != node_index) {
			rcu_read_lock();
			radix_tree_lookup_all(&mapping->page_tree, index, &node, &slot);
			rcu_read_unlock();
		}
		pbn = node->pbn[pbn_offset];
		if (pbn) {
			clear_bit(PBN_DELAY_CACHE_BIT, (void *)(&(node->pbn[pbn_offset])));
			if (PBN_is_delay_cache(node->pbn[pbn_offset]))
				printk("Release delay tree insert error!!\n");
		} else
			cluster_add_page_lbn_table(mapping, index);
	}
	/* Page re-allocation */
	for (count = 0; count < data->total_req_size; count++) {
		page = page_cache_alloc_readahead(mapping);
		if (!page) printk("Page reallocation error!!\n");
		list_add(&page->lru, &data->pagelist);
	}

	return 0;
}

struct notifier_block cluster_pagecache_syncio_processing_chain = {
	.notifier_call = cluster_pagecache_syncio_processing,
	.priority = 0,
};
struct notifier_block cluster_pagecache_asyncio_processing_chain = {
	.notifier_call = cluster_pagecache_asyncio_processing,
	.priority = 0,
};

struct page *cluster_do_page_cache_readahead(struct address_space *mapping, struct file *filp,
						pgoff_t offset, unsigned long nr_to_read, unsigned long lookahead_size)
{
	struct inode *inode = mapping->host;
	struct page *page;
	struct page *return_page = NULL;
	unsigned long end_index; 
	int page_idx;
	int ret = 0;
	loff_t isize = i_size_read(inode);

	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *pagelist = per_cpu_ptr(cluster_nvme->pagelist, smp_processor_id());
	struct notifier_data *data = &current->notifier_data;
	unsigned long prev_pbn = -1;
	INIT_LIST_HEAD(&data->pagelist);

	if (isize == 0)
		return NULL;

	end_index = ((isize - 1) >> PAGE_CACHE_SHIFT);
	if (end_index - offset + 1 < nr_to_read)
		nr_to_read = end_index - offset + 1;

	if (end_index == 0)
		return NULL;

	//printk("cpu %d sync readahead offset %d size %d ahead_size %d end_index %d\n",
	//			smp_processor_id(), offset, nr_to_read, lookahead_size, end_index);

	for (page_idx = 0; page_idx < nr_to_read; page_idx++) {
		struct radix_tree_node *node;
		void **slot;
		unsigned long pbn = 0;
		pgoff_t page_offset = offset + page_idx;
		unsigned long pbn_offset = page_offset & RADIX_TREE_MAP_MASK;

		rcu_read_lock();
		page = radix_tree_lookup_all(&mapping->page_tree, page_offset, &node, &slot);
		if (node)
			pbn = node->pbn[pbn_offset];
		rcu_read_unlock();
		if (page && !radix_tree_exceptional_entry(page)) {
			if (page_idx == 0)
				return_page = page;
			continue;
		}

		if (node && pbn) {
			if (test_and_set_bit(PBN_DELAY_CACHE_BIT, (void *)(&(node->pbn[pbn_offset])))) {
				/* Someone will update page in corresponding index */
				printk("Delay cache update flag is already set\n");
				continue;
			}
		} else  /* Find PBN in extent_status_tree */
			cluster_find_pbn(inode, page_offset, &pbn);

		page = list_last_entry(pagelist, struct page, lru);
		list_del(&page->lru);
		list_add(&page->lru, &data->pagelist);
		__set_page_locked(page);
		page->index = page_offset;
		//if (prev_pbn + 1 != PBN_get_pbn(pbn)) {
		if (prev_pbn == -1 || prev_pbn + 1 != PBN_get_pbn(pbn)) {
			if (prev_pbn == -1) {
				__set_page_poll(page);
				return_page = page;
			}
			page->private = PBN_get_pbn(pbn);
		}
		prev_pbn = PBN_get_pbn(pbn);

		if (page_idx == nr_to_read - lookahead_size) {
			SetPageReadahead(page);
		}
		ret++;
	}

	if (ret) {
		data->total_req_size = ret;
		data->end_io = cluster_mpage_end_io;
		cluster_readpages(mapping, ret);
	}

	return return_page;
}

struct page *cluster_do_page_cache_async_readahead(struct address_space *mapping, struct file *filp,
							pgoff_t offset, unsigned long nr_to_read, unsigned long lookahead_size)
{
	struct inode *inode = mapping->host;
	struct page *page;
	unsigned long end_index; 
	int page_idx;
	int ret = 0;
	loff_t isize = i_size_read(inode);

	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *pagelist = per_cpu_ptr(cluster_nvme->pagelist, smp_processor_id());
	struct notifier_data *data = &current->notifier_data;
	unsigned long prev_pbn = -1;
	INIT_LIST_HEAD(&data->pagelist);

	if (isize == 0)
		return NULL;

	end_index = ((isize - 1) >> PAGE_CACHE_SHIFT);
	if (end_index - offset + 1 < nr_to_read)
		nr_to_read = end_index - offset + 1;

	if (end_index == 0)
		return NULL;

	//printk("cpu %d ASYNC readahead offset %d size %d ahead_size %d end_index %d\n",
	//					smp_processor_id(), offset, nr_to_read, lookahead_size, end_index);

	for (page_idx = 0; page_idx < nr_to_read; page_idx++) {
		struct radix_tree_node *node;
		void **slot;
		unsigned long pbn = 0;
		pgoff_t page_offset = offset + page_idx;
		unsigned long pbn_offset = page_offset & RADIX_TREE_MAP_MASK;

		rcu_read_lock();
		page = radix_tree_lookup_all(&mapping->page_tree, page_offset, &node, &slot);
		if (node)
			pbn = node->pbn[pbn_offset];
		rcu_read_unlock();
		if (page && !radix_tree_exceptional_entry(page))
			continue;

		if (node && pbn) {
			if (test_and_set_bit(PBN_DELAY_CACHE_BIT, (void *)(&(node->pbn[pbn_offset])))) {
				/* Someone will update page in corresponding index */
				printk("Delay cache update flag is already set\n");
				continue;
			}
		} else  /* Find PBN in extent_status_tree */
			cluster_find_pbn(inode, page_offset, &pbn);

		page = list_last_entry(pagelist, struct page, lru);
		list_del(&page->lru);
		list_add(&page->lru, &data->pagelist);
		__set_page_locked(page);
		page->index = page_offset;
		if (prev_pbn == -1 || prev_pbn + 1 != PBN_get_pbn(pbn)) {
			//if (prev_pbn != -1) printk("PBN in-contiguous!!!!!\n");
			if (prev_pbn == -1) {
				__set_page_poll(page);
			}
			page->private = PBN_get_pbn(pbn);
		}
		prev_pbn = PBN_get_pbn(pbn);

		if (page_idx == nr_to_read - lookahead_size)
			SetPageReadahead(page);
		ret++;
	}

	if (ret) {
		data->total_req_size = ret;
		data->end_io = cluster_mpage_async_end_io;
		cluster_async_readpages(mapping, ret);
	}

	return NULL;
}

struct page *cluster_force_page_cache_readahead(struct address_space *mapping, struct file *filp,
											pgoff_t offset, unsigned long nr_to_read)
{
	struct page *page = NULL;

	if (unlikely(!(filp->f_flags & O_NVME)))
		return NULL;

	nr_to_read = min(nr_to_read, inode_to_bdi(mapping->host)->ra_pages);
	while (nr_to_read) {
		unsigned long this_chunk = (2 * 1024 * 1024) / PAGE_CACHE_SIZE;

		if (this_chunk > nr_to_read)
			this_chunk = nr_to_read;
		page = cluster_do_page_cache_readahead(mapping, filp, offset, this_chunk, 0);
		offset += this_chunk;
		nr_to_read -= this_chunk;
	}
	return page;
}

unsigned long cluster_get_init_ra_size(unsigned long size, unsigned long max)
{
	unsigned long newsize = roundup_pow_of_two(size);

	if (newsize <= max / 32)
		newsize = newsize * 4;
	else if (newsize <= max / 4)
		newsize = newsize * 2;
	else
		newsize = max;

	return newsize;
}

unsigned long cluster_get_next_ra_size(struct file_ra_state *ra, unsigned long max)
{
	unsigned long cur = ra->size;
	unsigned long newsize;

	if (cur < max / 16)
		newsize = 4 * cur;
	else
		newsize = 2 * cur;

	return min(newsize, max);
}

static pgoff_t count_history_pages(struct address_space *mapping,
				   pgoff_t offset, unsigned long max)
{
	pgoff_t head;

	rcu_read_lock();
	head = page_cache_prev_hole(mapping, offset - 1, max);
	rcu_read_unlock();

	return offset - 1 - head;
}

static int try_context_readahead(struct address_space *mapping,
				 struct file_ra_state *ra,
				 pgoff_t offset,
				 unsigned long req_size,
				 unsigned long max)
{
	pgoff_t size;

	size = count_history_pages(mapping, offset, max);

	/*
	 * not enough history pages:
	 * it could be a random read
	 */
	if (size <= req_size)
		return 0;

	/*
	 * starts from beginning of file:
	 * it is a strong indication of long-run stream (or whole-file-read)
	 */
	if (size >= offset)
		size *= 2;

	ra->start = offset;
	ra->size = min(size + req_size, max);
	ra->async_size = 1;

	return 1;
}

struct page *cluster_ondemand_readahead(struct address_space *mapping, struct file_ra_state *ra,
					struct file *filp, bool hit_readahead_mark, pgoff_t offset, unsigned long req_size)
{
	unsigned long max = ra->ra_pages;
	pgoff_t prev_offset;
	int async = 0;

	if (hit_readahead_mark)
		async = 1;

	/* start of file */
	if (!offset)
		goto initial_readahead;

	if ((offset == (ra->start + ra->size - ra->async_size) ||
	     offset == (ra->start + ra->size))) {
		ra->start += ra->size;
		ra->size = cluster_get_next_ra_size(ra, max);
		ra->async_size = ra->size;
		goto readit;
	}

	/*
	 * Hit a marked page without valid readahead state.
	 * E.g. interleaved reads.
	 * Query the pagecache for async_size, which normally equals to
	 * readahead size. Ramp it up and use it as the new readahead size.
	 */
	if (hit_readahead_mark) {
		pgoff_t start;

		rcu_read_lock();
		start = page_cache_next_hole(mapping, offset + 1, max);
		rcu_read_unlock();

		if (!start || start - offset > max)
			return 0;

		ra->start = start;
		ra->size = start - offset;  /* old async_size */
		ra->size += req_size;
		ra->size = cluster_get_next_ra_size(ra, max);
		ra->async_size = ra->size;
		goto readit;
	}

	/* oversize read */
	if (req_size > max)
		goto initial_readahead;

	/*
	 * sequential cache miss
	 * trivial case: (offset - prev_offset) == 1
	 * unaligned reads: (offset - prev_offset) == 0
	 */
	prev_offset = (unsigned long long)ra->prev_pos >> PAGE_CACHE_SHIFT;
	if (offset - prev_offset <= 1UL)
		goto initial_readahead;

	/*
	 * Query the page cache and look for the traces(cached history pages)
	 * that a sequential stream would leave behind.
	 */
	if (try_context_readahead(mapping, ra, offset, req_size, max))
		goto readit;

	/*
	 * standalone, small random read
	 * Read as is, and do not pollute the readahead state.
	 */
	return cluster_do_page_cache_readahead(mapping, filp, offset, req_size, 0);


 initial_readahead:
	ra->start = offset;
	ra->size = cluster_get_init_ra_size(req_size, max);
	ra->async_size = ra->size > req_size ? ra->size - req_size : ra->size;
 readit:
	if (offset == ra->start && ra->size == ra->async_size) {
		ra->async_size = cluster_get_next_ra_size(ra, max);
		ra->size += ra->async_size;
	}

	if (async) {
		cluster_do_page_cache_async_readahead(mapping, filp, ra->start, ra->size, ra->async_size);
		return NULL;
	}
	return cluster_do_page_cache_readahead(mapping, filp, ra->start, ra->size, ra->async_size);

}

struct page *cluster_page_cache_sync_readahead(struct address_space *mapping, struct file_ra_state *ra,
						struct file *filp, pgoff_t offset, unsigned long req_size)
{
	/* no read-ahead */
	if (!ra->ra_pages)
		return NULL;

	/* be dumb */
	if (filp && (filp->f_mode & FMODE_RANDOM)) {
		return cluster_force_page_cache_readahead(mapping, filp, offset, req_size);
	}

	/* do read-ahead */
	return cluster_ondemand_readahead(mapping, ra, filp, false, offset, req_size);
}

void cluster_page_cache_async_readahead(struct address_space *mapping, struct file_ra_state *ra,
						struct file *filp, struct page *page,  pgoff_t offset, unsigned long req_size)
{
	if (!ra->ra_pages)
		return;

	if (PageWriteback(page))
		return;

	ClearPageReadahead(page);

	if (inode_read_congested(mapping->host))
		return;

	cluster_ondemand_readahead(mapping, ra, filp, true, offset, req_size);
}

struct page *cluster_readpage(struct address_space *mapping, pgoff_t index)
{
	struct NVME_cluster *cluster_nvme = get_NVME_cluster();
	struct list_head *pagelist = per_cpu_ptr(cluster_nvme->pagelist, smp_processor_id());
	struct page *page;
	unsigned long pbn = 0;
	struct notifier_data *data = &current->notifier_data;
	INIT_LIST_HEAD(&data->pagelist);

	//printk("cpu %d single read offset %lu\n", smp_processor_id(), index);

	cluster_find_pbn(mapping->host, index, &pbn);
	page = list_last_entry(pagelist, struct page, lru);
	list_del(&page->lru);
	list_add(&page->lru, &data->pagelist);
	page->index = index;
	page->private = PBN_get_pbn(pbn);
	__set_page_locked(page);
	__set_page_poll(page);
	data->end_io = cluster_mpage_end_io;
	
	atomic_notifier_chain_register(current->page_cache_chain, &cluster_pagecache_syncio_processing_chain);
	nvme_direct_readpages(mapping->host, 1);

	return page;
}

void cluster_readpages(struct address_space *mapping, unsigned int nr_pages)
{
	atomic_notifier_chain_register(current->page_cache_chain, &cluster_pagecache_syncio_processing_chain);
	nvme_direct_readpages(mapping->host, nr_pages);

	return;
}

void cluster_async_readpages(struct address_space *mapping, unsigned int nr_pages)
{
	atomic_notifier_chain_register(current->page_cache_chain, &cluster_pagecache_asyncio_processing_chain);
	nvme_direct_async_readpages(mapping->host, nr_pages);

	return;
}

