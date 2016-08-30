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

void clflush_buffer_add(void *s, void *e);
void clflush_buffer_flush(void);
void clflush_buffer_clean(void);
int non_cc_quiescence_check(void *addr, u64_t timestamp);

#endif /* NON_CC_H */
