#include <linux/nvme_cluster.h>
#include "/lib/modules/4.4.7CLUSTER+/source/fs/ext4/ext4.h"
#include "cluster_debug.h"
#include "cluster_flag.h"

int check_flag(unsigned int value, unsigned int flag) {
	switch(flag){
		case DELAY:
			return value & EXT4_MAP_DELAYED;
		case HOLE:
			return value & EXT4_MAP_HOLE;
		case WRITTEN:
			return value & EXT4_MAP_MAPPED;
		case UNWRITTEN:
			return value & EXT4_MAP_UNWRITTEN;
		default:
			DEBUG_PRINT(ERROR,"check flag\n");
			return -1;
	}
}

unsigned int cluster_get_delay_flag(void) {
	return EXT4_MAP_DELAYED;
}
EXPORT_SYMBOL(cluster_get_delay_flag);

unsigned int cluster_get_hole_flag(void) {
	return EXT4_MAP_HOLE;
}

void cluster_set_delay_flag(unsigned int *flag) {
	*flag |= EXT4_MAP_DELAYED;
}

void cluster_set_hole_flag(unsigned int *flag) {
	*flag |= EXT4_MAP_HOLE;
}

unsigned int cluster_check_delay_flag(unsigned int flag) {
	return check_flag(flag, DELAY);
}

unsigned int cluster_check_hole_flag(unsigned int flag) {
	return check_flag(flag, HOLE);
}

unsigned int PBN_status(unsigned long long pbn) {
	return pbn >> PBN_SHIFT;
}
unsigned int PBN_type(unsigned long long pbn) {
	return (pbn & PBN_FLAG_MASK) >> PBN_SHIFT;
}
int PBN_is_delay(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_DELAY) != 0;
}
int PBN_is_hole(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_HOLE) != 0;
}
int PBN_is_written(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_WRITTEN) != 0;
}
int PBN_is_unwritten(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_UNWRITTEN) != 0;
}
int PBN_is_delay_unwritten(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_DELAY_UNWRITTEN) != 0;
}
int PBN_is_initial(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_INITIAL) != 0;
}
int PBN_is_init_and_delay(unsigned long long pbn) {
	return (PBN_type(pbn) & (PBN_DELAY | PBN_INITIAL)) != 0;
}
int PBN_is_init_and_hole(unsigned long long pbn) {
	return (PBN_type(pbn) & (PBN_HOLE | PBN_INITIAL)) != 0;
}
int PBN_is_init_and_written(unsigned long long pbn) {
	return (PBN_type(pbn) & (PBN_WRITTEN | PBN_INITIAL)) != 0;
}
int PBN_is_init_and_unwritten(unsigned long long pbn) {
	return (PBN_type(pbn) & (PBN_UNWRITTEN | PBN_INITIAL)) != 0;
}
int PBN_is_init_and_delay_unwritten(unsigned long long pbn) {
	return (PBN_type(pbn) & (PBN_DELAY_UNWRITTEN | PBN_INITIAL)) != 0;
}
int PBN_is_delay_cache(unsigned long long pbn) {
	return (PBN_type(pbn) & PBN_DELAY_CACHE) != 0;
}

unsigned long long PBN_get_pbn(unsigned long long pbn) {
	return pbn & ~PBN_MASK;
}

void PBN_set_pbn(unsigned long long *pbn, unsigned long long want) {
	*pbn = ((want & ~PBN_MASK) | (*pbn & PBN_MASK));
}
void PBN_set_flag(unsigned long long *pbn, unsigned int flag) {
	*pbn = (((unsigned long long)flag << PBN_SHIFT) & PBN_MASK) | (*pbn & ~PBN_MASK);
}

unsigned long long PBN_join_flag_pbn(unsigned long long pbn, unsigned int flag) {
	return (unsigned long long) ((((unsigned long long)flag << PBN_SHIFT) & PBN_MASK) | (pbn & ~PBN_MASK));
}

unsigned long long PBN_join_flag_pbn_init(unsigned long long pbn, unsigned int flag) {
	return (unsigned long long) ((((unsigned long long)(flag | PBN_INITIAL) << PBN_SHIFT) & PBN_MASK) | (pbn & ~PBN_MASK));
}

