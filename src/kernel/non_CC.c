#include "non_CC.h"
#include "assert.h"

struct non_cc_quiescence *cc_quiescence;
struct clflush_item clflush_buffer[NUM_CLFLUSH_ITEM];
int clflush_buffer_cnt = 0;

static inline void
clflush_range(void *s, void *e)
{
	s = (void *)round_to_cacheline(s);
	e = (void *)round_to_cacheline(e);
	for(; s<=e; s += CACHE_LINE) cos_flush_cache(s);
}

void
clflush_buffer_flush(void)
{
	struct clflush_item *buf = clflush_buffer;
	int i;

	for(i=0; i<clflush_buffer_cnt; i++, buf++) clflush_range(buf->start, buf->end);
}

void
clflush_buffer_add(void *s, void *e)
{
	assert(clflush_buffer_cnt < NUM_CLFLUSH_ITEM);
	clflush_buffer[clflush_buffer_cnt].start = s;
	clflush_buffer[clflush_buffer_cnt].end   = e;
	clflush_buffer_cnt++;
}

void clflush_buffer_clean(void) { clflush_buffer_cnt = 0; }

int
non_cc_quiescence_check(void *addr, u64_t timestamp)
{
	u32_t idx, i, quiescent = 1;
	struct non_cc_quiescence *ccq;

	idx = GET_QUIESCE_IDX(addr);
	ccq = &cc_quiescence[idx];
	clflush_range(ccq, ccq+1);
	for (i = 0; i < NUM_NODE; i++) {
		if (timestamp > ccq->last_mandatory_flush[i]) {
			quiescent = 0;
			break;
		}
	}
	return quiescent;
}
