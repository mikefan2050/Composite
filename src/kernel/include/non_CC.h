#ifndef NON_CC_H
#define NON_CC_H

#include "shared/util.h"
#include "shared/cos_types.h"

#define NUM_CLFLUSH_ITEM 100
#define GET_QUIESCE_IDX(va) (((u32_t)(va) - ivshmem_addr) / RETYPE_MEM_SIZE) 

struct non_cc_quiescence {
	u64_t last_mandatory_flush[NUM_NODE];
} __attribute__((aligned(CACHE_LINE), packed)) ;

struct clflush_item {
	void *start, *end;
};

extern struct non_cc_quiescence *cc_quiescence;
extern u64_t *global_tsc;

void clflush_buffer_add(void *s, void *e);
void clflush_buffer_flush(void);
void clflush_buffer_clean(void);
int non_cc_quiescence_check(void *addr, u64_t timestamp);
static inline void
non_cc_rdtscll(u64_t *t)
{
	if (!cur_node) {
		rdtscll(*global_tsc);
		cos_wb_cache(global_tsc);
	} else {
		cos_flush_cache(global_tsc);
	}
	*t = *global_tsc;
	cos_wb_cache(t);
}

static inline int 
cos_non_cc_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	assert(VA_IN_IVSHMEM_RANGE(target));
	return cos_cas(target, old, updated);
}

static inline int 
cos_non_cc_faa(int *var, int value)
{
	assert(VA_IN_IVSHMEM_RANGE(var));
	return cos_faa(var, value);
}

static inline void
cos_clflush_range(void *s, void *e)
{
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e);
	for(; s<=e; s += CACHE_LINE) cos_flush_cache(s);
}

static inline void
cos_clwb_range(void *s, void *e)
{
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e);
	for(; s<=e; s += CACHE_LINE) cos_wb_cache(s);
}

#endif /* NON_CC_H */
