#ifndef MICRO_BOOTER_H
#define MICRO_BOOTER_H
#include <stdio.h>
#include <string.h>

#undef assert
#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_thd_switch(termthd);} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define BUG_DIVZERO() do { debug_print("Testing divide by zero fault @ "); int i = num / den; } while (0);

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#define ITER 1000000
#define CACHELINE_SIZE  64
#define PACKET_SIZE (16*PAGE_SIZE)

enum boot_pmem_captbl_layout {
	BOOT_CLIENT           = 2,
	BOOT_SERVER           = 4,
	BOOT_PMEM_CAPTBL_FREE = round_up_to_pow2(BOOT_SERVER, CAPMAX_ENTRY_SZ)
};

struct meta_header {
	char magic[MAGIC_LEN];
	int kernel_done, boot_done, node_num, boot_num;
	u64_t global_tsc;
};
extern struct meta_header *ivshmem_meta;
extern int cur_node;

extern struct cos_compinfo booter_info;
extern thdcap_t termthd; 		/* switch to this to shutdown */
extern int num, den;

extern int prints(char *s);
extern int printc(char *fmt, ...);

static inline
int call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (arg1), "S" (arg2), "D" (arg3) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

/* load/store a value of other nodes */
static inline int
non_cc_load_int(int *target)
{
	cos_flush_cache(target);
	return *target;
}

static inline void
non_cc_store_int(int *target, int value)
{
	*target = value;
	cos_wb_cache(target);
}

static inline void
clflush_range(void *s, void *e)
{
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e);
	for(; s<=e; s += CACHE_LINE) cos_flush_cache(s);
}

static inline void
clwb_range(void *s, void *e)
{
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e);
	for(; s<=e; s += CACHE_LINE) cos_wb_cache(s);
}

#endif /* MICRO_BOOTER_H */
