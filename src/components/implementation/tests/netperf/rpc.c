#include "rpc.h"

static struct msg_pool global_msg_pool;
static struct shared_page ret_page[NUM_NODE];

void *
rpc_create(int node_mem, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16, n;
	void *addr;
	volatile struct create_ret *ret = (struct create_ret *)ret_page[caller].addr;

//	printc("rpc create node %d\n", caller);
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
	struct mem_meta * mem;
	void *addr;

//	printc("rpc send sender %d to %d id %d sz %d\n", caller, recv_node, memid, size);
	mem = mem_lookup(memid);
	assert(size <= mem->size);
	addr = (void *)mem->addr;
	clwb_range(addr, addr+size);
	meta.mem_id = memid;
	meta.size   = size;
	ret = msg_enqueue(&global_msg_pool.nodes[recv_node].recv[caller], &meta);
	return ret;
}

void *
rpc_recv(int node_mem, int spin)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	struct recv_ret *ret = (struct recv_ret *)ret_page[caller].addr;
	int deq, i;
	struct msg_meta meta;
	struct mem_meta * mem;
	void *addr;

//	printc("rpc recv node %d\n", caller);
	do {
		for(i=(caller+1)%NUM_NODE; i!=caller; i = (i+1)%NUM_NODE) {
			deq = msg_dequeue(&global_msg_pool.nodes[caller].recv[i], &meta);
			if (!deq) {
				ret->mem_id = meta.mem_id;
				ret->size   = meta.size;
				ret->sender = i;
				ret->addr   = mem_retrieve(meta.mem_id, caller);
				mem         = mem_lookup(meta.mem_id);
				assert(meta.size <= mem->size);
				addr = (void *)mem->addr;
				clflush_range(addr, addr+meta.size);
				return ret_page[caller].dst;
			}
		}
	} while(spin);
	return NULL;
}

void *
rpc_call(int node_mem, int recv_node, int size)
{
	int r;

	do {
		r = rpc_send(node_mem, recv_node, size);
	} while(r);

	return rpc_recv(node_mem, 1);
}

int
rpc_wait_replay(int node_mem, int size)
{
	int caller = node_mem & 0xFFFF, memid = node_mem >> 16;
	struct recv_ret *ret = (struct recv_ret *)ret_page[caller].addr;
	void *r;

	r = rpc_recv(node_mem, 1);
	assert(r);

	return rpc_send(node_mem, ret->sender, size);
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

	printc("rpc register\n");

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

	printc("rpc init node %d addr %x size %x\n", caller, untype, size);
	mem_mgr_init(untype, size, (vaddr_t)cos_get_heap_ptr()+PAGE_SIZE);
	for(i=0; i<NUM_NODE; i++) {
		for(j=0; j<NUM_NODE; j++) {
			global_msg_pool.nodes[i].recv[j].head = 0;
			global_msg_pool.nodes[i].recv[j].tail = 0;
		}
	}
}

