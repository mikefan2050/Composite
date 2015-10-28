#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

int
prints(char *s)
{
    int len = strlen(s);
	  cos_print(s, len);
	  return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_print(s, ret);

	  return ret;
}

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_thd_switch();} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

struct cos_compinfo booter_info;

static void
thd_fn(void *d)
{
	printc("\tNew thread %d with argument %d\n", cos_thdid(), (int)d);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	printc("Error, shouldn't get here!\n");
}

#define TEST_NTHDS 5
static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		assert(ts[i]);
		printc("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	printc("test done\n");
}

static void
test_mem(void)
{
	char *p = cos_page_bump_alloc(&booter_info);

	assert(p);
	strcpy(p, "victory");

	printc("Page allocation: %s\n", p);
}

volatile arcvcap_t rcc_global, rcp_global;
volatile asndcap_t scp_global;

static void
async_thd_fn(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	unsigned long a, b;
	int pending;

	printc("Asynchronous event thread handler.\n<-- rcving...\n");
	pending = cos_rcv(rc, &a, &b);
	printc("<-- pending %d, %lx, %lx\n<-- rcving...\n", pending, a, b);
	pending = cos_rcv(rc, &a, &b);
	printc("<-- pending %d, %lx, %lx\n<-- rcving...\n", pending, a, b);
	pending = cos_rcv(rc, &a, &b);
	printc("<-- Error: manually returning to snding thread.\n");
	cos_thd_switch(tc);
	printc("ERROR: in async thd *after* switching back to the snder.\n");
	while (1) ;
}

static void
async_thd_parent(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	int ret, pending;
	unsigned long a, b;

	printc("--> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("asnd returned %d.\n", ret);
	printc("--> Back in the asnder.\n--> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("--> asnd returned %d.\n", ret);
	printc("--> Back in the asnder.\n--> receiving to get notifications\n");
	pending = cos_rcv(rc, &a, &b);
	printc("--> pending %d, %lx, %lx\n", pending, a, b);

	cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
	thdcap_t tcp, tcc;
	arcvcap_t rcp, rcc;

	printc("Creating threads, and async end-points.\n");
	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcp);
	rcp = cos_arcv_alloc(&booter_info, tcp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void*)tcp);
	assert(tcc);
	rcc = cos_arcv_alloc(&booter_info, tcc, booter_info.comp_cap, rcp);
	assert(rcc);

	/* make the snd channel to the child */
	scp_global = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global);

	rcc_global = rcc;
	rcp_global = rcp;

	cos_thd_switch(tcp);

	printc("Async end-point test successful.\nTest done.\n");
}

static void
spinner(void *d)
{ while (1) ; }

static void
test_timer(void)
{
	int i;
	thdcap_t tc;

	printc("Starting timer test.\n");
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0 ; i < 10 ; i++) {
		unsigned long a, b;

		printc(".");
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &a, &b);
		cos_thd_switch(tc);
	}

	printc("Timer test completed.\nSuccess.\n");
}

int
test_serverfn(int a, int b, int c)
{ return a + b + c; }

extern void *__inv_test_serverfn(int a, int b, int c);

static inline
int call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (arg1), "S" (arg2), "D" (arg3) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

static void
test_inv(void)
{
	compcap_t cc;
	sinvcap_t ic;
	unsigned int r;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);

	r = call_cap_mb(ic, 1, 2, 3);
	printc("Return from invocation: %d\n", r);
}

void
cos_init(void)
{
	printc("\nMicro Booter started.\n");

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	printc("---------------------------\n");
	test_thds();
	printc("---------------------------\n");
//	test_timer();
	printc("---------------------------\n");
	test_mem();
	printc("---------------------------\n");
	test_async_endpoints();
	printc("---------------------------\n");
	test_inv();
	printc("---------------------------\n");

	printc("\nMicro Booter done.\n");

//	while (1) ;
	BUG();

	return;
}
