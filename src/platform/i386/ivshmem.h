#ifndef IVSHMEM_H
#define IVSHMEM_H

#include <shared/cos_types.h>
#include <liveness_tbl.h>
#include <pgtbl.h>
#include <thd.h>
#include <component.h>

#define IVSHMEM_MAGIC "IVSHMEM"
#define IVSHMEM_TOT_SIZE     PGD_SIZE*32
#define IVSHMEM_UNTYPE_START PGD_SIZE*8
#define IVSHMEM_UNTYPE_SIZE  PGD_SIZE*8

struct ivshmem_meta {
	int kernel_done, boot_done, node_num;
	struct liveness_entry *pmem_liveness_tbl;
	struct retype_info_glb *pmem_glb_retype_tbl;
	struct retype_info *pmem_retype_tbl[NUM_NODE];
	pgtbl_t pmem_pgd[NUM_NODE];
};
extern paddr_t ivshmem_phy_addr;
extern unsigned long ivshmem_sz;
extern vaddr_t ivshmem_addr;
extern struct ivshmem_meta *meta_page;
extern int cur_node;

u8_t *ivshmem_boot_alloc(unsigned int size);
void ivshmem_set_page(u32_t page);
void ivshmem_boot_init(struct captbl *ct);

#endif /* IVSHMEM_H */
