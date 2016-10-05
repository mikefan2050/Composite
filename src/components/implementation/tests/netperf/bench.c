#include "bench.h"
#include "rpc.h"

static void *buf;
static int memid;

void
client_start(int cur)
{
	int i, r, msg_sz = PACKET_SIZE;
	struct create_ret *crt_ret;
	struct recv_ret *rcv_ret;
	void *rcv;
	unsigned long long start, end;

	cur_node = cur;
	printc("I am client heap %p cur_node %d\n", cos_get_heap_ptr(), cur_node);
	printc("meta %p magic %s done %d\n", ivshmem_meta, ivshmem_meta->magic, ivshmem_meta->boot_done);
	call_cap_mb(RPC_REGISTER, cur_node, 2, 3);
	crt_ret = (struct create_ret *)call_cap_mb(RPC_CREATE, cur_node, msg_sz, 3);
	buf     = crt_ret->addr;
	memid   = crt_ret->mem_id;
	assert(buf);

	rcv_ret = (struct recv_ret *)call_cap_mb(RPC_RECV, cur_node, 1, 0);
	rdtscll(start);
	for(i=0; i<ITER; i++) {
		((int *)buf)[0] = i;
		r = call_cap_mb(RPC_SEND, (memid << 16) | cur_node, 1, msg_sz);
		assert(!r);

		rcv_ret = (struct recv_ret *)call_cap_mb(RPC_RECV, cur_node, 1, 0);
		assert(rcv_ret);
		rcv = rcv_ret->addr;
		assert(rcv);
		assert(100+i == ((int *)rcv)[0]);
	}
	rdtscll(end);
	printc("rpc sz %d avg round trip %llu\n", msg_sz, (end-start)/ITER);

	return ;
}

void
server_start(int cur)
{
	int i, r, msg_sz = PACKET_SIZE;
	volatile struct create_ret *crt_ret;
	volatile struct recv_ret *rcv_ret;
	void *rcv;

	cur_node = cur;
	printc("I am server node %d meta %p magic %s done %d\n", cur_node, ivshmem_meta, ivshmem_meta->magic, ivshmem_meta->boot_done);
	call_cap_mb(RPC_REGISTER, cur_node, 2, 3);
	crt_ret = (struct create_ret *)call_cap_mb(RPC_CREATE, cur_node, msg_sz, 3);
	buf     = crt_ret->addr;
	memid   = crt_ret->mem_id;
	assert(buf);

	r = call_cap_mb(RPC_SEND, (memid << 16) | cur_node, 0, msg_sz);

	for(i=0; i<ITER; i++) {
		rcv_ret = (struct recv_ret *)call_cap_mb(RPC_RECV, cur_node, 1, 0);
		assert(rcv_ret);
		rcv = rcv_ret->addr;
		assert(rcv);
		assert(i == ((int *)rcv)[0]);

		((int *)buf)[0] = 100+((int *)rcv)[0];
		r = call_cap_mb(RPC_SEND, (memid << 16) | cur_node, 0, msg_sz);
		assert(!r);
//		printc("server send %d recv %d\n", ((int *)buf)[0], ((int *)rcv)[0]);
	}
	return ;
}

