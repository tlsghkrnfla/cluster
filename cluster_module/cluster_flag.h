#ifndef __CLUSTER_FLAG_H
#define __CLUSTER_FLAG_H

#define DELAY	0
#define HOLE	1
#define WRITTEN	2
#define UNWRITTEN	3
#define DELAY_UNWRITTEN 4
#define INITIAL	5
#define DELAY_CACHE 6
#define FLAG_NUM 7

#define PBN_SHIFT	(sizeof(unsigned long long)*8 - FLAG_NUM)
#define PBN_MASK	(~((unsigned long long)0) << PBN_SHIFT)  // 11111110....0 (64bit)

#define PBN_DELAY	(1 << DELAY)
#define PBN_HOLE	(1 << HOLE)
#define PBN_WRITTEN	(1 << WRITTEN)
#define PBN_UNWRITTEN	(1 << UNWRITTEN)
#define PBN_DELAY_UNWRITTEN (1 << DELAY_UNWRITTEN)
#define PBN_INITIAL	(1 << INITIAL)
#define PBN_DELAY_CACHE	(1 << DELAY_CACHE)

#define PBN_FLAG_MASK	((unsigned long long)(PBN_DELAY | \
				PBN_HOLE | \
				PBN_WRITTEN | \
				PBN_UNWRITTEN | \
				PBN_DELAY_UNWRITTEN | \
				PBN_INITIAL | \
				PBN_DELAY_CACHE) << PBN_SHIFT)

#define PBN_DELAY_CACHE_BIT 63

int check_flag(unsigned int, unsigned int);
unsigned int cluster_get_delay_flag(void);
unsigned int cluster_get_hole_flag(void);
void cluster_set_delay_flag(unsigned int*);
void cluster_set_hole_flag(unsigned int*);
unsigned int cluster_check_delay_flag(unsigned int);
unsigned int cluster_check_hole_flag(unsigned int);

struct pbn_status
{
	int pbn_unwritten;
	int pbn_written;
	int pbn_hole;
	int pbn_delay;
	int pbn_delay_unwritten;
	int pbn_initial;
	int pbn_uninitial;
	int pbn_delay_cache;
	int pbn_total;
};


unsigned int PBN_status(unsigned long long pbn);
unsigned int PBN_type(unsigned long long pbn);
int PBN_is_delay(unsigned long long pbn);
int PBN_is_hole(unsigned long long pbn);
int PBN_is_written(unsigned long long pbn);
int PBN_is_unwritten(unsigned long long pbn);
int PBN_is_delay_unwritten(unsigned long long pbn);
int PBN_is_initial(unsigned long long pbn);
int PBN_is_init_and_delay(unsigned long long pbn);
int PBN_is_init_and_hole(unsigned long long pbn);
int PBN_is_init_and_written(unsigned long long pbn);
int PBN_is_init_and_unwritten(unsigned long long pbn);
int PBN_is_init_and_delay_unwritten(unsigned long long pbn);
int PBN_is_delay_cache(unsigned long long pbn);
unsigned long long PBN_get_pbn(unsigned long long pbn);
void PBN_set_pbn(unsigned long long *pbn, unsigned long long want);
void PBN_set_flag(unsigned long long *pbn, unsigned int flag);
unsigned long long PBN_join_flag_pbn(unsigned long long pbn, unsigned int flag);
unsigned long long PBN_join_flag_pbn_init(unsigned long long pbn, unsigned int flag);
#endif
