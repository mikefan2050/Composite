#include "ivshmem.h"
#include "string.h"
#include "chal.h"

paddr_t ivshmem_phy_addr;
size_t ivshmem_sz;
vaddr_t ivshmem_addr=0, ivshmem_bump;
struct ivshmem_meta *meta_page;
int cur_node;
extern u8_t *boot_comp_pgd;
extern int boot_nptes(unsigned int sz);

u8_t *
ivshmem_boot_alloc(unsigned int size)
{
	assert(ivshmem_bump < ivshmem_addr+IVSHMEM_UNTYPE_START);
	u8_t *r = (u8_t *)ivshmem_bump;
	size = round_to_page(size);
	ivshmem_bump += size;
/*	u8_t *r = glb_memlayout.kern_boot_heap;
	unsigned long i;

	assert(glb_memlayout.allocs_avail);

	glb_memlayout.kern_boot_heap += npages * (PAGE_SIZE/sizeof(u8_t));
	assert(glb_memlayout.kern_boot_heap <= mem_kmem_end());
	for (i = (unsigned long)r ; i < (unsigned long)glb_memlayout.kern_boot_heap ; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2kern((void*)chal_va2pa((void*)i))) {
				die("Retyping to kernel on boot-time heap allocation failed @ 0x%x.\n", i);
			}
		}
	}*/
	return r;
}

int
ivshmem_pgtbl_mappings_add(struct captbl *ct, capid_t pgdcap, capid_t ptecap, const char *label,
			void *kern_vaddr, unsigned long user_vaddr, unsigned int range, int uvm)
{
	int ret;
	u8_t *ptes;
	unsigned int nptes = 0, i;
	struct cap_pgtbl *pte_cap, *pgd_cap;
	pgtbl_t pgtbl;

	pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, pgdcap);
	if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
	pgtbl = (pgtbl_t)pgd_cap->pgtbl;
	nptes = boot_nptes(round_up_to_pgd_page(user_vaddr+range)-round_to_pgd_page(user_vaddr));
	ptes = ivshmem_boot_alloc(nptes*PAGE_SIZE);
	assert(ptes);
	printk("\tCreating %d %s PTEs for PGD @ 0x%x from [%x,%x) to [%x,%x).\n",
	       nptes, label, chal_pa2va((paddr_t)pgtbl),
	       kern_vaddr, kern_vaddr+range, user_vaddr, user_vaddr+range);

	pte_cap = (struct cap_pgtbl*)captbl_lkup(ct, ptecap);
	assert(pte_cap);

	/* Hook in the PTEs */
	for (i = 0 ; i < nptes ; i++) {
		u8_t   *p  = ptes + i * PAGE_SIZE;

		pgtbl_init_pte(p);
		pte_cap->pgtbl = (pgtbl_t)p;

		/* hook the pte into the boot component's page tables */
		ret = cap_cons(ct, pgdcap, ptecap, (capid_t)(user_vaddr + i*PGD_RANGE));
		assert(!ret);
	}

	printk("\tMapping in %s.\n", label);
	/* Map in the actual memory. */
	for (i = 0 ; i < range/PAGE_SIZE ; i++) {
		u8_t *p     = kern_vaddr + i * PAGE_SIZE;
		paddr_t pf  = chal_va2pa(p);
		u32_t mapat = (u32_t)user_vaddr + i * PAGE_SIZE, flags = 0;

		if (uvm  && pgtbl_mapping_add(pgtbl, mapat, pf, PGTBL_USER_DEF)) assert(0);
		if (!uvm && pgtbl_cosframe_add(pgtbl, mapat, pf, PGTBL_COSFRAME)) assert(0);
		assert((void*)p == pgtbl_lkup(pgtbl, user_vaddr+i*PAGE_SIZE, &flags));
	}

	return 0;
}


void
ivshmem_set_page(u32_t page)
{
//	if (ivshmem_addr) return;
	ivshmem_addr = page * (1 << 22);
	meta_page    = (struct ivshmem_meta *)ivshmem_addr;
	ivshmem_bump = ivshmem_addr+PAGE_SIZE;
	printk("ivshmem phy %x virtul %x sz %d\n", ivshmem_phy_addr, ivshmem_addr, ivshmem_sz);
//	printk("ivshmem got set up %s\n", (char *)ivshmem_addr);
/*	if (strncmp((char *)ivshmem_addr, "IVSHMEM", 7)) {
		memcpy((char *)ivshmem_addr, "IVSHMEM", 8);
	} else {
		printk("ivshmem got set up %s\n", (char *)ivshmem_addr);
	}*/
}

void
ivshmem_boot_init(struct captbl *ct)
{
	int i, j, nkmemptes, ret = 0;
	pgtbl_t pgtbl;

	if (meta_page->kernel_done) {
		printk("ivshmem kernel done!\n");
		cur_node = meta_page->node_num++;
		if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PMEM_PT_BASE+(1+cur_node)*CAP32B_IDSZ, meta_page->pmem_pgd[cur_node], 0)) assert(0);
		return ;
	}
	cur_node = 0;
	meta_page->node_num = 1;

	meta_page->pmem_liveness_tbl = (struct liveness_entry *)ivshmem_boot_alloc(sizeof(__liveness_tbl));
	ltbl_init(meta_page->pmem_liveness_tbl);

	meta_page->pmem_glb_retype_tbl = (struct retype_info_glb *)ivshmem_boot_alloc(sizeof(glb_retype_tbl));
	for (i = 0; i < NUM_NODE; i++) {
		meta_page->pmem_retype_tbl[i] = (struct retype_info *)ivshmem_boot_alloc(sizeof(struct retype_info));
	}
	for (i = 0; i < NUM_NODE; i++) {
		for (j = 0; j < N_MEM_SETS; j++) {
			meta_page->pmem_retype_tbl[i]->mem_set[j].refcnt_atom.type    = RETYPETBL_UNTYPED;
			meta_page->pmem_retype_tbl[i]->mem_set[j].refcnt_atom.ref_cnt = 0;
			meta_page->pmem_retype_tbl[i]->mem_set[j].last_unmap          = 0;
		}
	}
	for (i = 0; i < N_MEM_SETS; i++) {
		meta_page->pmem_glb_retype_tbl[i].type = RETYPETBL_UNTYPED;
	}

	for (i = 1; i <= NUM_NODE; i++) {
		pgtbl = (pgtbl_t)ivshmem_boot_alloc(PAGE_SIZE);
		memset(pgtbl, 0, PAGE_SIZE);
		memcpy((void *)pgtbl + KERNEL_PGD_REGION_OFFSET, (void *)&boot_comp_pgd + KERNEL_PGD_REGION_OFFSET, KERNEL_PGD_REGION_SIZE);
		pgtbl = (pgtbl_t)chal_va2pa(pgtbl);
		meta_page->pmem_pgd[i-1] = pgtbl;
		if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PMEM_PT_BASE+i*CAP32B_IDSZ, pgtbl, 0)) assert(0);
		ret = ivshmem_pgtbl_mappings_add(ct, BOOT_CAPTBL_PMEM_PT_BASE+i*CAP32B_IDSZ, BOOT_CAPTBL_KM_PTE, "untyped memory", 
					(void *)ivshmem_addr+IVSHMEM_UNTYPE_START+i*IVSHMEM_UNTYPE_SIZE, BOOT_MEM_KM_BASE, IVSHMEM_UNTYPE_SIZE, 0);
		assert(ret == 0);
	}

	meta_page->kernel_done = 1;
}
