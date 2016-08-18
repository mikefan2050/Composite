#ifndef IVSHMEM_H
#define IVSHMEM_H

#include <shared/cos_types.h>
#include <liveness_tbl.h>
#include <pgtbl.h>
#include <thd.h>
#include <component.h>

#define IVSHMEM_UNTYPE_START PGD_SIZE*8

struct ivshmem_meta {
	char magic[MAGIC_LEN];
	int kernel_done, boot_done, node_num, boot_num;
	struct liveness_entry *pmem_liveness_tbl;
	struct retype_info_glb *pmem_glb_retype_tbl;
	struct retype_info *pmem_retype_tbl;
	pgtbl_t pmem_pgd[NUM_NODE];
	struct captbl *pmem_ct[NUM_NODE];
};
extern unsigned long ivshmem_sz;
extern struct ivshmem_meta *meta_page;

u8_t *ivshmem_boot_alloc(unsigned int size);
void ivshmem_set_page(u32_t page);
void ivshmem_boot_init(struct captbl *ct);

#endif /* IVSHMEM_H */
