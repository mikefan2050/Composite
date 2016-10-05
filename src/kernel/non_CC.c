#include "include/non_CC.h"
#include "include/pgtbl.h"

#define TOT_PTE_NUM (PAGE_SIZE/sizeof(unsigned long))
#define CC_PTE_NUM  (TOT_PTE_NUM/2)
struct non_cc_quiescence *cc_quiescence;
u64_t local_quiescence[NUM_NODE];
u64_t *global_tsc, clflush_start;
extern int ivshmem_pgd_idx, ivshmem_pgd_end;
extern u32_t *boot_comp_pgd;
int cur_pgd_idx, cur_pte_idx, npage_flush;

static inline int
__cc_quiescence_local_check(u64_t timestamp)
{
	int i, quiescent = 1;
	for (i = 0; i < NUM_NODE; i++) {
		if (timestamp > local_quiescence[i]) {
			quiescent = 0;
			break;
		}
	}
	return quiescent;
}

static inline void
copy_cc_quiescence(void)
{
	int i;

	for(i=0; i<NUM_NODE; i++) cos_flush_cache(&cc_quiescence[i].last_mandatory_flush);
	for(i=0; i<NUM_NODE; i++) local_quiescence[i] = cc_quiescence[i].last_mandatory_flush;
}

static int
non_cc_quiescence_check(u64_t timestamp)
{
	if (!__cc_quiescence_local_check(timestamp)) {
		copy_cc_quiescence();
	}
	return __cc_quiescence_local_check(timestamp);
}

int
cos_quiescence_check(u64_t cur, u64_t past, u64_t grace_period, quiescence_type_t type)
{
	switch(type) {
	case TLB_QUIESCENCE:
		return tlb_quiescence_check(past);
	case KERNEL_QUIESCENCE:
		return QUIESCENCE_CHECK(cur, past, grace_period);
	case NON_CC_QUIESCENCE:
		return non_cc_quiescence_check(past);
	}
	return 0;
}

int
cos_cache_mandatory_flush(void)
{
	unsigned long *pte, page;
	void *addr;
	int i, j, ret = 0;
	u32_t *kernel_pgtbl = (u32_t *)&boot_comp_pgd;

	if (cur_pgd_idx == ivshmem_pgd_idx && !cur_pte_idx) {
		non_cc_rdtscll(&clflush_start);
		npage_flush = 0;
	}
	pte = chal_pa2va(kernel_pgtbl[cur_pgd_idx] & PGTBL_FRAME_MASK);
	for(i=cur_pte_idx, j=0; i < (int)TOT_PTE_NUM && j < (int)CC_PTE_NUM; i++, j++) {
		page = pte[i];
		if (page & PGTBL_ACCESSED) {
			addr = chal_pa2va(page & PGTBL_FRAME_MASK);
			cos_clflush_range(addr, addr+PAGE_SIZE);
			pte[i] &= (~PGTBL_ACCESSED);
			npage_flush++;
		}
	}
	if (i == TOT_PTE_NUM) {
		cur_pte_idx = 0;
		cur_pgd_idx++;
		if (cur_pgd_idx == ivshmem_pgd_end) {
			cur_pgd_idx = ivshmem_pgd_idx;
		}
	} else {
		cur_pte_idx = i;
	}
	if (cur_pgd_idx == ivshmem_pgd_idx && !cur_pte_idx) {
		cc_quiescence[cur_node].last_mandatory_flush = clflush_start;
		cos_wb_cache(&cc_quiescence[cur_node].last_mandatory_flush);
		ret = npage_flush;
	}
	return ret;
}

void
non_cc_init(void)
{
	cur_pgd_idx = ivshmem_pgd_idx;
	cur_pte_idx = 0;
	npage_flush = 0;
}
