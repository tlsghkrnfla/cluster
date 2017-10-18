#ifndef __CLUSTER_DEBUG_H
#define __CLUSTER_DEBUG_H
#define ERROR 0
#define INFO 1
void DEBUG_PRINT(int, const char*);
#define DEBUG_ENTRY {}
#define DEBUG_EXIT {}
//#define DEBUG_ENTRY printk("%s(line %d): %s entry\n", __FILE__, __LINE__, __FUNCTION__)
//#define DEBUG_EXIT printk("%s(line %d): %s exit\n", __FILE__, __LINE__, __FUNCTION__)
#endif
