obj-m = cluster.o
cluster-y = cluster_module.o cluster_main.o cluster_debug.o cluster_flag.o cluster_open.o cluster_read.o cluster_readahead.o cluster_tmp.o
cluster-y += nvme_main.o nvme_get_qpair.o nvme_mapping.o nvme_request.o scsi.o nvme_direct_io.o
KVERSION = $(shell uname -r)
BLDDIR = /lib/modules/$(KVERSION)/build

all:
	make -C $(BLDDIR) M=$(PWD) modules

clean:
	make -C $(BLDDIR) M=$(PWD) clean 
