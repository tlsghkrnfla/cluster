#include <linux/printk.h>
#include "cluster_debug.h"
void DEBUG_PRINT(int info, const char * string){
	switch(info){
		case ERROR:
			printk("[Error] %s", string);
			return;
		case INFO:
			printk("[INFO] %s\n", string);
			return;
	}
}
