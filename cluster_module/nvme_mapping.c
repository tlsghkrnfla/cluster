/*
//Byeonghun Hyeon <gusqudgnskku@gmail.com>
*/
#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include "nvme.h"

/*
static int cpu_to_queue_index(unsigned int nr_cpus, unsigned int nr_queues,
			      const int cpu)
{
	return cpu * nr_queues / nr_cpus;
}
*/

static int get_first_sibling(unsigned int cpu)
{
	unsigned int ret;

	ret = cpumask_first(topology_sibling_cpumask(cpu));
	if (ret < nr_cpu_ids)
		return ret;
	return cpu;
}

static int NVME_update_interrupt_queue_map(unsigned int *i_map, unsigned int nr_queues,
								const struct cpumask *online_mask)
{
	unsigned int i, nr_cpus, nr_uniq_cpus, queue, first_sibling;
	cpumask_var_t cpus;

	if (!alloc_cpumask_var(&cpus, GFP_ATOMIC))
		return 1;

	cpumask_clear(cpus);
	nr_cpus = nr_uniq_cpus = 0;
	for_each_cpu(i, online_mask) {
		nr_cpus++;
		first_sibling = get_first_sibling(i);
		if (!cpumask_test_cpu(first_sibling, cpus))
			nr_uniq_cpus++;
		cpumask_set_cpu(i, cpus);
	}

	queue = 0;
	for_each_possible_cpu(i) {
		if (!cpumask_test_cpu(i, online_mask)) {
			i_map[i] = 0;
			continue;
		}
		if (nr_queues >= nr_cpus || nr_cpus == nr_uniq_cpus) {
			//i_map[i] = cpu_to_queue_index(nr_cpus, nr_queues, queue);]
			i_map[i] = i + 96;
			queue++;
			continue;
		}
		first_sibling = get_first_sibling(i);
		if (first_sibling == i) {
			//i_map[i] = cpu_to_queue_index(nr_uniq_cpus, nr_queues, queue);
			i_map[i] = i + 96;
			queue++;
		} else {
			i_map[i] = i_map[first_sibling];
		}
	}

	free_cpumask_var(cpus);
	return 0;
}

static int NVME_update_polling_queue_map(unsigned int *p_map, unsigned int nr_queues,
						const struct cpumask *online_mask)
{
	unsigned int i, nr_cpus, nr_uniq_cpus, queue, first_sibling;
	cpumask_var_t cpus;

	if (!alloc_cpumask_var(&cpus, GFP_ATOMIC))
		return 1;

	cpumask_clear(cpus);
	nr_cpus = nr_uniq_cpus = 0;
	for_each_cpu(i, online_mask) {
		nr_cpus++;
		first_sibling = get_first_sibling(i);
		if (!cpumask_test_cpu(first_sibling, cpus))
			nr_uniq_cpus++;
		cpumask_set_cpu(i, cpus);
	}

	queue = 0;
	for_each_possible_cpu(i) {
		if (!cpumask_test_cpu(i, online_mask)) {
			p_map[i] = 0;
			continue;
		}
		if (nr_queues >= nr_cpus || nr_cpus == nr_uniq_cpus) {
			p_map[i] = i + 112;  /* #112 ~ #127 polling queue */
			queue++;
			continue;
		}
printk("polling queue make error\n");
		first_sibling = get_first_sibling(i);
		if (first_sibling == i) {
			p_map[i] = i + 112;
			queue++;
		} else {
			p_map[i] = p_map[first_sibling];
		}
	}

	free_cpumask_var(cpus);
	return 0;
}

unsigned int *NVME_make_cq_map(struct nvme_dev *dev, int num_queue, int node)
{
	unsigned int *interrupt_map;
	unsigned int *polling_map;

	interrupt_map = kzalloc_node(sizeof(*interrupt_map) * nr_cpu_ids, GFP_KERNEL, node);
	if (!interrupt_map)
		return NULL;
	polling_map = kzalloc_node(sizeof(*polling_map) * nr_cpu_ids, GFP_KERNEL, node);
	if (!polling_map)
		return NULL;

	if (!NVME_update_polling_queue_map(polling_map, num_queue, cpu_online_mask)) {
		dev->polling_map = polling_map;
		if (!NVME_update_interrupt_queue_map(interrupt_map, num_queue, cpu_online_mask)) {
			dev->interrupt_map = interrupt_map;
			return 0;
		}
		dev->polling_map = NULL;
		kfree(polling_map);
	}

	return NULL;
}

