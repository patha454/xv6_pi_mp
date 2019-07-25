/*****************************************************************
*       main.c
*       by Zhiyi Huang, hzy@cs.otago.ac.nz
*       University of Otago
*
********************************************************************/


#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "arm.h"
#include "mailbox.h"

#include <uspi.h>
#include <uspios.h>

extern char end[]; // first address after kernel loaded from ELF file
//extern pde_t *kpgdir;
extern volatile uint *mailbuffer;
extern unsigned int pm_size;
volatile extern u32 cpu_sig[];

void basic_delay(uint n) {
  uint sum;
  volatile uint i;
  for (i = 0; i < n; i++) {
    sum += i;
  }
}

void OkLoop()
{
   setgpiofunc(12, 1); // gpio 18 for Ok Led, set as an output
   setgpioval(12, 1);
   while(1){
        setgpioval(12, 1);
        basic_delay(2000000);
        setgpioval(12, 0);
        basic_delay(2000000);
   }
}

void NotOkLoop()
{
   setgpiofunc(12, 1); // gpio 18 for Ok Led, set as an output
   while(1){
        setgpioval(12, 1);
        basic_delay(500000);
        setgpioval(12, 0);
        basic_delay(500000);
   }
}

unsigned int getpmsize()
{
    create_request(mailbuffer, MPI_TAG_GET_ARM_MEMORY, 8, 0, 0);
    writemailbox((uint *)mailbuffer, 8);
    readmailbox(8);
    if(mailbuffer[1] != 0x80000000) cprintf("Error readmailbox: %x\n", MPI_TAG_GET_ARM_MEMORY);
    return mailbuffer[MB_HEADER_LENGTH + TAG_HEADER_LENGTH+1];
}

void machinit(void)
{
  int cpu;
  memset(cpus, 0, sizeof(struct cpu)*NCPU);
  for (cpu = 0; cpu < NCPU; cpu++) {
    cpus[cpu].id = cpu;
    cpus[cpu].first_sched = 1;
  }
}


void enableirqminiuart(void);

int lock __attribute((aligned (64))) = 0;

void locktest(void) {
  invalidate_dcache_range(&lock, 4);
  cprintf("Lock value: 0x%x\n", lock);
  cprintf("Lock addr: 0x%x\n", &lock);
  cprintf("Locking...\n");
  spin_acquire((void *) &lock);
  cprintf("locked\n");
  cprintf("Lock value: 0x%x\n", lock);
  spin_release((void *) &lock);
  cprintf("Unlocked\n");
  cprintf("Lock value: 0x%x\n", lock);
}

/**
 * Blocks the current CPU until all other cores report 
 * particular status.
 */
void wait_on_cores(enum cpustate target)
{
  while (1) {
    int ready = 1;
    uchar local_cpu = curr_cpu->id;
    for (int i = 0; i < NCPU; i++) {
      if (i == local_cpu) {
	continue;
      }
      if (cpu_sig[i] != target) {
	ready = 0;
      }
    }
    if (ready) {
      break;
    }
  }
}
  

void startothers(void)
{
  curr_cpu->started = CENTRY;
  signal_event();
  cprintf("CPU %x: Starting other cores\n", cpu_id());
  invalidate_dcache_range((void*) cpu_sig, 20);
  wait_on_cores(CENTRY);
}


/**
 * Configure and start secondary CPUs.
 * aux_main assumes the primary CPU has already
 * initialised shared OS data structures.
 */
void aux_main(void)
{
  cpu_sig[curr_cpu->id] = CENTRY;
  cprintf("CPU %d: Booted\n", curr_cpu->id);
  wait_for_event();
  tvinit();
  cprintf("CPU %d: tvinit Ok.\n", curr_cpu->id);
  basic_delay(1000000);
  aux_mmu_init();
  //Can secondary cores see the primary core tvint alloc? Move tvinit earlier in main?
  cprintf("CPU %d: aux_mmu_init: Ok.\n", curr_cpu->id);
  cprintf("CPU %d: Handing off to scheduler.\n", curr_cpu->id);
  scheduler();
}


int cmain()
{
    mmuinit0();
    machinit();
    #if defined (RPI1) || defined (RPI2)
    uartinit();
    #elif defined (FVP)
    uartinit_fvp();
    #endif
    dsb_barrier();
    consoleinit();
    cprintf("\nHello World from xv6\n");
    kinit1(end, P2V((8*1024*1024)+PHYSTART));
    cprintf("kinit1: Ok\n");
    mailboxinit();
    cprintf("Mailbox init: Ok\n");
    gpuinit();
    cprintf("GPU init: Ok\n");
    // collect some free space (8 MB) for imminent use
    // the physical space below 0x8000 is reserved for PGDIR and kernel stack
    //kpgdir=p2v(K_PDX_BASE);
    curr_cpu->kpgdir = p2v(K_PDX_BASE);
    
    pm_size = getpmsize();;
    cprintf("ARM memory: %x\n", pm_size);
    startothers();
    mmuinit1();
    cprintf("mmuinit1: OK\n");
    //Alert secondary cores to copy the page table and contiue.
    signal_event();
    cprintf("ARM xv6 MP USB\n");
    pinit();
    cprintf("pinit: OK\n");
    tvinit();
    cprintf("tvinit: OK\n");
    binit();
    cprintf("binit: OK\n");
    fileinit();
    cprintf("fileinit: OK\n");
    iinit();
    cprintf("iinit: OK\n");
    ideinit();
    cprintf("ideinit: OK\n");
    kinit2(P2V((8*1024*1024)+PHYSTART), P2V(pm_size));
    cprintf("kinit2: OK\n");
    usbinit();
    userinit();
    cprintf("userinit: OK\n");
    timer3init();
    cprintf("timer3init: OK\n");
    enableirqminiuart();
    cprintf("Handing off to scheduler...\n");
    
    scheduler();
    NotOkLoop();
    return 0;
}
