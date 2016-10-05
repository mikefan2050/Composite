#include "rpc.h"
#include "test.h"

static struct msg_pool global_msg_pool;
static struct shared_page ret_page[NUM_NODE];

void *
rpc_create(int node_mem, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16, n;
	void *addr;
	volatile struct create_ret *ret = (struct create_ret *)ret_page[caller].addr;

//	printc("rpc create\n");
	size  = round_up_to_page(size);
	n     = size/PAGE_SIZE;
	addr  = alloc_pages(n);
	memid = mem_create(addr, size);
	addr  = mem_retrieve(memid, caller);
	ret->addr = addr;
	ret->mem_id = memid;

	return ret_page[caller].dst;
}

int
rpc_connect(int node_mem, int recv_node, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;

	printc("rpc connect\n");
	return 0;
}

int
rpc_send(int node_mem, int recv_node, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	int ret;
	struct msg_meta meta;
#ifdef NON_CC_OP
	struct mem_meta * mem;
	void *addr;

//	printc("rpc send sender %d to %d id %d sz %d\n", caller, recv_node, memid, size);
	mem = mem_lookup(memid);
	assert(size <= mem->size);
	addr = (void *)mem->addr;
	clwb_range(addr, addr+size);
#endif
	meta.mem_id = memid;
	meta.size   = size;
	ret = msg_enqueue(&global_msg_pool.nodes[recv_node].recv[caller], &meta);
	return ret;
}

void *
rpc_recv(int node_mem, int spin)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	volatile struct recv_ret *ret = (struct recv_ret *)ret_page[caller].addr;
	int deq, i;
	struct msg_meta meta;
	struct mem_meta * mem;
	void *addr;

//	printc("rpc recv node %d\n", caller);
	do {
		for(i=0; i<NUM_NODE; i++) {
			deq = msg_dequeue(&global_msg_pool.nodes[caller].recv[i], &meta);
			if (!deq) {
				ret->mem_id = meta.mem_id;
				ret->size   = meta.size;
				ret->sender = i;
				ret->addr   = mem_retrieve(meta.mem_id, caller);
#ifdef NON_CC_OP
				mem         = mem_lookup(meta.mem_id);
				assert(meta.size <= mem->size);
				addr = (void *)mem->addr;				
				clflush_range(addr, addr+meta.size);
#endif
				return ret_page[caller].dst;
			}
		}
	} while(spin);
	return NULL;
}

int
rpc_free(int node_mem, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;

	printc("rpc free\n");
	return 0;
}

void
rpc_register(int node_mem)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	void *addr, *dst;

	printc("rpc register caller %d\n", caller);

	addr = alloc_pages(1);
	dst  = alias_pages(caller, addr, 1);
	ret_page[caller].addr = addr;
	ret_page[caller].dst  = dst;
	((char *)addr)[4095] = '$';
}

void
rpc_init(int node_mem, vaddr_t untype, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	int i, j;

	printc("rpc init node %d addr %x size %x vas %x\n", caller, untype, size, (vaddr_t)cos_get_heap_ptr()+PAGE_SIZE);
	mem_mgr_init(untype, size, (vaddr_t)cos_get_heap_ptr()+PAGE_SIZE);
	for(i=0; i<NUM_NODE; i++) {
		for(j=0; j<NUM_NODE; j++) {
			global_msg_pool.nodes[i].recv[j].head = 0;
			global_msg_pool.nodes[i].recv[j].tail = 0;
		}
	}
}

