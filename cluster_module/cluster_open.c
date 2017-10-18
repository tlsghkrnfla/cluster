#include <linux/kernel.h>
#include <linux/pagemap.h>
#include <linux/list_lru.h>
#include <linux/fs.h>
#include <linux/radix-tree.h>
#include <linux/swap.h>
#include "/lib/modules/4.4.7CLUSTER+/source/fs/ext4/ext4.h"
#include "cluster_flag.h"
#include "cluster_debug.h"

//#include <linux/nvme_cluster.h>
//#include "nvme.h"
//#include <trace/events/latency.h>

static int __cluster_build_page_lbn_table(struct address_space *);
static int page_lbn_table_insert(struct address_space *, struct ext4_map_blocks *, unsigned int, int);
static int __fill_in_page_cache_lbn(struct address_space *, struct ext4_map_blocks *, unsigned int, int);

static int page_lbn_table_insert(struct address_space *mapping, struct ext4_map_blocks *map,
						unsigned int flag, int init)
{
	struct radix_tree_node *node;
	void **slot;
	int error;
	unsigned int lbn = map->m_lblk;
	unsigned int len = map->m_len;
	unsigned long long pbn = map->m_pblk;
	unsigned int index = lbn;
	unsigned int end_index = lbn + len;
	unsigned int offset;
	unsigned int total = 0;

	DEBUG_ENTRY;
	while (index < end_index) {
		error = radix_tree_create_with_offset(&mapping->page_tree, index, &node, &slot, &offset);
		if (error) {
			DEBUG_PRINT(ERROR,"radix_tree_create_with_offset is error\n");
			return error;
		}
		if (!node) {
			if (lbn)
				DEBUG_PRINT(ERROR, "lbn should be 0\n");
			error = radix_tree_create_with_offset(&mapping->page_tree, lbn+1, &node, NULL, NULL);
			if (error) {
				DEBUG_PRINT(ERROR,"radix_tree_create_with_offset is error\n");
				return error;
			}
		}
		if (node) {
			node->pbn[offset] = PBN_join_flag_pbn_init(pbn, flag);
			if (!PBN_is_initial(node->pbn[offset]))
				DEBUG_PRINT(ERROR, "page_pbn tree initial\n");
			if (!list_empty(&node->private_list))
				list_lru_del(&workingset_shadow_nodes, &node->private_list);
			pbn++;
			offset++;
			index++;
			total++;
			while (offset < RADIX_TREE_MAP_SIZE) {
				if(index < end_index) {
					node->pbn[offset] = PBN_join_flag_pbn_init(pbn, flag);
					if(!PBN_is_initial(node->pbn[offset]))
						DEBUG_PRINT(ERROR, "page_pbn tree initial\n");
					offset++;
					index++;
					pbn++;
					total++;
				} else
					return total;
			}
		} else {
			DEBUG_PRINT(ERROR, "should be more thing\n");
			return 0;
		}
	}
	DEBUG_EXIT;

	return total;
}

static int __fill_in_page_cache_lbn(struct address_space *mapping,
				struct ext4_map_blocks *map, unsigned int flag, int init)
{
	int error;

	DEBUG_ENTRY;
	error = radix_tree_maybe_preload(mapping_gfp_constraint(mapping, GFP_KERNEL) & ~__GFP_HIGHMEM);
	if (error)
		return error; 
	spin_lock_irq(&mapping->tree_lock);
	error = page_lbn_table_insert(mapping, map, flag, init);
	spin_unlock_irq(&mapping->tree_lock);
	if (unlikely(error))
		return error;
	DEBUG_EXIT;

	return 0;
}

static int fill_in_page_cache_lbn(struct address_space *mapping,
				struct ext4_map_blocks *map, int init)
{
	unsigned int flag = map->m_flags;

	DEBUG_ENTRY;
	if (check_flag(flag, WRITTEN))  // mapping O storage data O
		__fill_in_page_cache_lbn(mapping, map, PBN_WRITTEN, init);
	else if (check_flag(flag, HOLE))  // mapping X storage data X
		__fill_in_page_cache_lbn(mapping, map, PBN_HOLE, init);
	else if (check_flag(flag, UNWRITTEN) && !check_flag(flag, DELAY))  // mapping O data X
		__fill_in_page_cache_lbn(mapping, map, PBN_UNWRITTEN, init);
	else if (check_flag(flag, UNWRITTEN) && check_flag(flag, DELAY))  // mapping O data will be O
		__fill_in_page_cache_lbn(mapping, map, PBN_DELAY_UNWRITTEN, init);
	else if (!check_flag(flag, UNWRITTEN) && check_flag(flag, DELAY))  // data will be changed
		__fill_in_page_cache_lbn(mapping, map, PBN_DELAY, init);
	else {
		DEBUG_PRINT(ERROR, "fill_in_page_cache_lbn\n");

printk("error offset %d len %d\n", map->m_lblk, map->m_len);


		DEBUG_EXIT;
		return -1;
	}
	DEBUG_EXIT;

	return 0;
}

static int __cluster_build_page_lbn_table(struct address_space *mapping)
{
	struct inode *inode = mapping->host;
	const unsigned blkbits = inode->i_blkbits;
	const unsigned blocksize = 1 << blkbits;
	sector_t block_in_file;
	//sector_t last_block_in_file;
	sector_t last_block;
	struct ext4_map_blocks map;

	DEBUG_ENTRY;
	block_in_file = 0;
	//last_block_in_file = (i_size_read(inode) + blocksize - 1) >> blkbits;
	last_block = (i_size_read(inode) + blocksize - 1) >> blkbits;
	//last_block = 64;
	while (block_in_file < last_block) {
		map.m_lblk = block_in_file;
		map.m_len = last_block - block_in_file;
		if (ext4_map_blocks(NULL, inode, &map, 0) < 0) {
			DEBUG_PRINT(ERROR, "ext4_map_blocks\n");
			return -1;
		}
		fill_in_page_cache_lbn(mapping, &map, 0);
		block_in_file += map.m_len;
	}  
	DEBUG_EXIT;

	return 0;
}

int cluster_add_page_lbn_table(struct address_space *mapping, pgoff_t index)
{
	struct inode *inode = mapping->host;
	const unsigned blkbits = inode->i_blkbits;
	const unsigned blocksize = 1 << blkbits;
	sector_t block_in_file;
	sector_t last_block_in_file;
	sector_t last_block;
	struct ext4_map_blocks map;

	DEBUG_ENTRY;
	block_in_file = (index >> 6) << 6;
	last_block_in_file = (i_size_read(inode) + blocksize - 1) >> blkbits;
	last_block = block_in_file + 64;
	if (last_block_in_file < last_block)
		last_block = last_block_in_file;

	while (block_in_file < last_block) {
		map.m_lblk = block_in_file;
		map.m_len = last_block - block_in_file;
		if (ext4_map_blocks(NULL, inode, &map, 0) < 0) {
			DEBUG_PRINT(ERROR, "ext4_map_blocks\n");
			return -1;
		}
		fill_in_page_cache_lbn(mapping, &map, 1);
		block_in_file += map.m_len;
	}  
	DEBUG_EXIT;

	return 0;
}

int cluster_build_page_lbn_table(struct address_space *mapping)
{
	//printk("rnode : %p / height : %u\n", mapping->page_tree.rnode, mapping->page_tree.height);
	__cluster_build_page_lbn_table(mapping);
	//printk("after rnode :%p / height : %u\n", mapping->page_tree.rnode, mapping->page_tree.height);

	return 0;
}

