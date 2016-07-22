#ifndef IVSHMEM_H
#define IVSHMEM_H

#include "ivshmem.h"
#include "string.h"
#include "chal.h"

paddr_t ivshmem_phy_addr;
size_t ivshmem_sz;
vaddr_t ivshmem_addr;

void
ivshmem_set_page(u32_t page)
{
	ivshmem_addr = page * (1 << 22);
	printk("ivshmem phy %x virtul %x sz %d\n", ivshmem_phy_addr, ivshmem_addr, ivshmem_sz);
	if (strncmp((char *)ivshmem_addr, "IVSHMEM", 7)) {
		memcpy((char *)ivshmem_addr, "IVSHMEM", 8);
	} else {
		printk("ivshmem got set up %s\n", (char *)ivshmem_addr);
	}
}
#endif /* IVSHMEM_H */
