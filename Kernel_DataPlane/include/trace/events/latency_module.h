#undef TRACE_SYSTEM
#define TRACE_SYSTEM latency_module

#if !defined(_TRACE_LATENCY_MODULE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_LATENCY_MOUDLE_H
#include <linux/tracepoint.h>

TRACE_EVENT(page_alloc_in,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_alloc_in %llu", (unsigned long long)__entry->rdtsc)
);


TRACE_EVENT(page_alloc_out,

	TP_PROTO(unsigned long long rdtsc),
	
	TP_ARGS(rdtsc),

	TP_STRUCT__entry(
		__field(unsigned long long, rdtsc)
	),

	TP_fast_assign(
		__entry->rdtsc = rdtsc;
	),

	TP_printk("page_alloc_out %llu", (unsigned long long)__entry->rdtsc)
);

#endif
#include <trace/define_trace.h>
