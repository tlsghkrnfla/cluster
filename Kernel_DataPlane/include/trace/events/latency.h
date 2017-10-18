#undef TRACE_SYSTEM
#define TRACE_SYSTEM latency

#if !defined(_TRACE_LATENCY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_LATENCY_H
#include <linux/tracepoint.h>

TRACE_EVENT(file_sys_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field( unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("file_sys_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(file_sys_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("file_sys_out %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(blk_sys_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("blk_sys_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(blk_sys_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("blk_sys_in %llu", (unsigned long long)__entry->rdtsc)
);

TRACE_EVENT(page_sys_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long ,rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_sys_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(page_sys_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_sys_out %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(nvme_sys_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("nvme_sys_in %llu", (unsigned long long)__entry->rdtsc)
);

TRACE_EVENT(nvme_sys_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("nvme_sys_out %llu", (unsigned long long)__entry->rdtsc)
);



TRACE_EVENT(read_sys_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("syscall_read_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(read_sys_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("syscall_read_out %llu", (unsigned long long)__entry->rdtsc)
);



TRACE_EVENT(nvme_doorbell,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("nvme_doorbell %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(req_completion_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("req_completion_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(req_completion_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("req_completion_out %llu", (unsigned long long)__entry->rdtsc)
);



TRACE_EVENT(add_lru_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("add_lru_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(add_lru_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("add_lru_out %llu", (unsigned long long)__entry->rdtsc)
);

TRACE_EVENT(schedule_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("schedule_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(schedule_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("schedule_out %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(page_sleep_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_sleep_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(page_sleep_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_sleep_out %llu", (unsigned long long)__entry->rdtsc)
);


#endif
#include <trace/define_trace.h>
