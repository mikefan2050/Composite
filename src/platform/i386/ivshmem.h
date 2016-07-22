#ifndef IVSHMEM_H
#define IVSHMEM_H

#include <shared/cos_types.h>

#define IVSHMEM_MAGIC "IVSHMEM"

extern paddr_t ivshmem_phy_addr;
extern unsigned long ivshmem_sz;
extern vaddr_t ivshmem_addr;

void ivshmem_set_page(u32_t page);

#endif /* IVSHMEM_H */
