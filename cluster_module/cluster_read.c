#include <linux/kernel.h>
#include <linux/uio.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/radix-tree.h>
#include <linux/printk.h>
#include <linux/swap.h>
#include <linux/nvme_cluster.h>
#include </lib/modules/4.4.7CLUSTER+/source/fs/ext4/ext4.h>
#include "cluster_flag.h"
#include "cluster_debug.h"
#include "cluster_tmp.h"
#include "cluster_readahead.h"
#include <trace/events/latency.h>

static void async_lazy_process(struct page *page)
{
	if (current->page_cache_chain->head) {
		__clear_page_poll(page);
		atomic_notifier_call_chain(current->page_cache_chain, 0, &current->notifier_data);
		atomic_notifier_call_chain(current->device_driver_chain, 0, &current->notifier_data);
		current->page_cache_chain->head = NULL;
		current->device_driver_chain->head = NULL;
	}
}

static int cluster_pbn_delay_cache(struct radix_tree_node *node, unsigned long pbn_offset)
{
	if (test_bit(PBN_DELAY_CACHE_BIT, (void *)(&(node->pbn[pbn_offset])))) {
			printk("PBN has delay cache flag? %lu\n", pbn_offset);
		while (PBN_is_delay_cache(((struct radix_tree_node *)node)->pbn[pbn_offset]));
			printk("PBN has delay cache flag!!! %lu\n", pbn_offset);
		return 1;
	} else
		return 0;
}

static struct page *__find_get_entry(struct address_space *mapping, pgoff_t index,
					struct radix_tree_node **node, void ***slot, unsigned long *pbn)
{
	struct page *page;
	unsigned long pbn_offset;

	rcu_read_lock();
 repeat:
	page = NULL;
	radix_tree_lookup_all(&mapping->page_tree, index, node, slot);
	if (*slot) {
		page = radix_tree_deref_slot(*slot);
		if (!page) {
			pbn_offset = index & RADIX_TREE_MAP_MASK;
			*pbn = (*node)->pbn[pbn_offset];
			goto out;
		}
		if (radix_tree_exception(page)) {
			if(radix_tree_deref_retry(page))
				goto repeat;
			goto out;
		}
		if (!page_cache_get_speculative(page))
			goto repeat;
		if (unlikely(page != **slot)) {
			page_cache_release(page);
			goto repeat;
		}
	}
 out:
	rcu_read_unlock();

	return page;
}

struct page *cluster_find_get_page(struct address_space *mapping, pgoff_t index,
									void **node, void ***slot, unsigned long *pbn)
{
	struct page *page;

	page = __find_get_entry(mapping, index, (struct radix_tree_node **)node, slot, pbn);
	if (radix_tree_exceptional_entry(page)) {
		page = NULL;
		return page;
	}

	return page;
}

int cluster_find_pbn(struct inode *inode, pgoff_t index, unsigned long *pbn)
{
	struct ext4_map_blocks map;

	map.m_lblk = index;
	map.m_len = 1;
	if (ext4_map_blocks(NULL, inode, &map, 0) < 0) {
		printk("ext4_map_blocks error\n");
		return -1;
	}
	*pbn = PBN_join_flag_pbn_init(map.m_pblk, PBN_WRITTEN);

	return 0;
}

ssize_t cluster_do_generic_file_read(struct file *filp, loff_t *ppos,
								struct iov_iter *iter, ssize_t written)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index;
	pgoff_t last_index;
	pgoff_t prev_index;
	unsigned long offset;
	unsigned int prev_offset;
	unsigned long pbn_offset;
	int error = 0;

	struct notifier_data *data = &current->notifier_data;
	ATOMIC_NOTIFIER_HEAD(page_cache_chain);
	ATOMIC_NOTIFIER_HEAD(device_driver_chain);
	ATOMIC_NOTIFIER_HEAD(polling_chain);
	current->page_cache_chain = &page_cache_chain;
	current->device_driver_chain = &device_driver_chain;
	current->polling_chain = &polling_chain;

	index = *ppos >> PAGE_CACHE_SHIFT;
	prev_index = ra->prev_pos >> PAGE_CACHE_SHIFT;
	prev_offset = ra->prev_pos & (PAGE_CACHE_SIZE-1);
	last_index = (*ppos + iter->count + PAGE_CACHE_SIZE -1) >> PAGE_CACHE_SHIFT;
	offset = *ppos & ~PAGE_CACHE_MASK;
	pbn_offset = index & RADIX_TREE_MAP_MASK;

//printk("cluster_do_generic_file_read index %d\n", index);

	for( ; ;) {
		struct page *page;
		pgoff_t end_index;
		loff_t isize;
		unsigned long nr, ret;
		void *node = NULL;
		void **slot;
		unsigned long pbn = 0;
		data->total_req_size = 0;
		data->total_req_num = 0;
		data->mapping = mapping;
		current->page_cache_chain->head = NULL;
		current->device_driver_chain->head = NULL;
		current->polling_chain->head = NULL;

		cond_resched();
 cluster_find_page:
		page = cluster_find_get_page(mapping, index, &node, &slot, &pbn);
		if (!page) {
			if (pbn) {
				if (cluster_pbn_delay_cache((struct radix_tree_node *)node, pbn_offset))
					goto cluster_find_page;
			}
			page = cluster_page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);
			if (unlikely(page == NULL)) {
				if (pbn)
					clear_bit(PBN_DELAY_CACHE_BIT, (void *)(&(((struct radix_tree_node *)
																node)->pbn[pbn_offset])));
				goto cluster_readpage;
			}
		}
		if (PageReadahead(page)) {
			//if (current->page_cache_chain->head) {
			//	printk("??? sync alread\n");
			//}
			cluster_page_cache_async_readahead(mapping, ra, filp, page, index, last_index - index);
			//async_lazy_process(page);
		}
		if (!PageUptodate(page)) {
			if (inode->i_blkbits != PAGE_CACHE_SHIFT)
				DEBUG_PRINT(ERROR, "read error\n");
			else
				goto cluster_page_not_up_to_date;
		}
		/* Check lazy processing */
		async_lazy_process(page);
 cluster_page_ok:
		isize = i_size_read(inode);
		end_index = (isize - 1) >> PAGE_CACHE_SHIFT;
		if (unlikely(!isize || index > end_index)) {
			page_cache_release(page);
			goto cluster_out;
		}

		nr = PAGE_CACHE_SIZE;
		if (index == end_index) {
			nr = ((isize - 1) & ~PAGE_CACHE_MASK) + 1;
			if (nr <= offset) {
				page_cache_release(page);
				goto cluster_out;
			}
		}
		nr = nr - offset;
		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);
		if (prev_index != index || offset != prev_offset)
			mark_page_accessed(page);
		prev_index = index;

		ret = copy_page_to_iter(page, offset, nr, iter);
		offset += ret;
		index += offset >> PAGE_CACHE_SHIFT;
		offset &= ~PAGE_CACHE_MASK;
		prev_offset = offset;
		page_cache_release(page);
		written += ret;

		if (!iov_iter_count(iter))
			goto cluster_out;
		if (ret < nr) {
			error = -EFAULT;
			goto cluster_out;
		}
		continue;
 cluster_page_not_up_to_date:
		/* Waiting until PG_locked bit is unset if page is locked
		   trylock_page : set bit & return 1(bit was 1) or return 0(bit was 0) */
		error = lock_page_killable(page);
		async_lazy_process(page);

		if (unlikely(error))
			goto cluster_readpage_error;
		/* I got a lock of page but, is there mapping?
		   Someone truncate the file (truncate can cut or expand file with NULL) */
		if (!page->mapping) {
			unlock_page(page);
			page_cache_release(page);
			continue;
		}

		if (PageUptodate(page)) {
			unlock_page(page);
			goto cluster_page_ok;
		}
 cluster_readpage:
		//ClearPageError(page);
		{
			page = cluster_readpage(mapping, index);
/*
		 	if (PBN_is_init_and_written(pbn)) {
				page = cluster_read_page(mapping, index);
			} else if(PBN_is_init_and_unwritten(pbn)) {
				// There isn't data in storage (mapping exist)
				zero_user_segment(page, 0, PAGE_CACHE_SIZE);
				SetPageUptodate(page);
				unlock_page(page);
			} else if(PBN_is_init_and_hole(pbn)) {
				// There isn't mapping with storage
				zero_user_segment(page, 0, PAGE_CACHE_SIZE);
				SetPageUptodate(page);
				unlock_page(page);
			} else if(PBN_is_init_and_delay(pbn)) {
				// There is data in cache but not in storage yet
				unlock_page(page);
			} else if(PBN_is_init_and_delay_unwritten(pbn)) {
				unlock_page(page);
			} else {
				unlock_page(page);
			}
*/
		}
 		goto cluster_page_not_up_to_date;
 cluster_readpage_error:
		page_cache_release(page);
		goto cluster_out;
	}
 cluster_out:
	ra->prev_pos = prev_index;
	ra->prev_pos <<= PAGE_CACHE_SHIFT;
	ra->prev_pos |= prev_offset;
	*ppos = ((loff_t)index << PAGE_CACHE_SHIFT) + offset;
	file_accessed(filp);

	return written? written : error;
}

//cluster_rdtsc(NVME_latency.lo, NVME_latency.hi, &NVME_latency.sync_end);

