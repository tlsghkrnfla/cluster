#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "cluster_main.h"
#include "cluster_debug.h"
#include "nvme_main.h"

int __init init_cluster_module(void) {
	DEBUG_ENTRY;
	init_cluster_main();
	init_nvme_main();
	DEBUG_EXIT;
	return 0;
}

void __exit exit_cluster_module(void) {
	DEBUG_ENTRY;
	exit_nvme_main();
	exit_cluster_main();
	DEBUG_EXIT;
	return;
}

module_init(init_cluster_module);
module_exit(exit_cluster_module);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Byeonghun Hyeon <gusqudgnskku@gmail.com>");
MODULE_VERSION("1.0");
