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
extern pde_t *kpgdir;
extern volatile uint *mailbuffer;
extern unsigned int pm_size;

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
    memset(cpus, 0, sizeof(struct cpu)*NCPU);
}


void enableirqminiuart(void);

int lock __attribute((aligned (64))) = 0;

void locktest(void) {
  int attrb = (kpgdir[PDX(&lock)] << 12) >> 12;
  invalidate_dcache_range(&lock, 4);
  cprintf("Lock mem attrb: 0x%x\n", attrb);
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

uint mb_data[10];

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
    kpgdir=p2v(K_PDX_BASE);

    pm_size = getpmsize();;
    cprintf("ARM memory: %x\n", pm_size);
    mmuinit1();
    cprintf("mmuinit1: OK\n");
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
