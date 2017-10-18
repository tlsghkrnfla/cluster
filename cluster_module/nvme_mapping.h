#ifndef __NVME_MAPPING_H
#define __NVME_MAPPING_H
/*
//Byeonghun Hyeon <gusqudgnskku@gmail.com>
*/

/*
//@num_queue 	: number of hardware queue
//@node		: memory area for mapping tale
//@return		: map_array
//--------------------------------------------------
//mapping table 
//(core_id) -> (hardware_queue_id)
*/

unsigned int *NVME_make_cq_map(struct nvme_dev *, int, int);
unsigned int *NVME_make_polling_map(int, int);

#endif
