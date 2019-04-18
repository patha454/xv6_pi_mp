/*****************************************************************
 *       mmu.c
 *       by Zhiyi Huang, hzy@cs.otago.ac.nz
 *       University of Otago
 *
 ********************************************************************/


#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"

unsigned int pm_size;

void mmuinit0(void)
{
	pde_t *l1;
	pte_t *l2;
	uint pa, va;

	// diable mmu
	// use inline assembly here as there is a limit on 
	// branch distance after mmu is disabled
	//	asm volatile("mrc p15, 0, r1, c1, c0, 0\n\t"
	//		"bic r1,r1,#0x00000004\n\t"	// 2:  Disable data cache
	//		"bic r1,r1,#0x00001000\n\t" // 12: Disable instruction cache
	//		"bic r1,r1,#0x00000800\n\t" // 11: Disable branch prediction
	//		"bic r1,r1,#0x00000001\n\t" // 0:  Disable MMU
	//		"mcr p15, 0, r1, c1, c0, 0\n\t"
	//		"mov r0, #0\n\t"
	//		"mcr p15, 0, r0, c7, c7, 0\n\t" // Invalidate Both Caches (only for ARM11)
	//		"mcr p15, 0, r0, c8, c7, 0\n\t" // Invalidate Unified TLB
	//		::: "r0", "r1", "cc", "memory");


	//for(p=(uint *)0x2000; p<(uint *)0x8000; p++) *p = 0;

	l1 = (pde_t *) K_PDX_BASE;
	l2 = (pte_t *) K_PTX_BASE;

	//Not required in AMRv8 - use assembly in entry.S
	// map all of ram at KERNBASE
	/*va = KERNBASE + MBYTE;
	for(pa = PHYSTART + MBYTE; pa < PHYSTART+PHYSIZE; pa += MBYTE){
	  l1[PDX(va)] = pa|DOMAIN0|PDX_AP(K_RW)|SECTION|CACHED|BUFFERED|L1_SHAREABLE;
		va += MBYTE;
	}*/

	// identity map first MB of ram so mmu can be enabled
	//l1[PDX(PHYSTART)] = PHYSTART|DOMAIN0|PDX_AP(K_RW)|SECTION|CACHED|BUFFERED;
	//cprintf("Kernel attrb: 0x%x\n", DOMAIN0|PDX_AP(K_RW)|SECTION|CACHED|BUFFERED|L1_SHAREABLE);
	// map IO region
	va = MMIO_VA;
	for(pa = MMIO_PA; pa < MMIO_PA+MMIO_SIZE; pa += MBYTE){
		l1[PDX(va)] = pa|DOMAIN0|PDX_AP(K_RW)|SECTION;
		va += MBYTE;
	}

	// map GPU memory
	va = GPUMEMBASE;
	for(pa = 0; pa < (uint)GPUMEMSIZE; pa += MBYTE){
		l1[PDX(va)] = pa|DOMAIN0|PDX_AP(K_RW)|SECTION;
		va += MBYTE;
	}

	// double map exception vectors at top of virtual memory
	va = HVECTORS;
	l1[PDX(va)] = (uint)l2|DOMAIN0|COARSE;
	l2[PTX(va)] = PHYSTART|PTX_AP(K_RW)|SMALL;

	flush_dcache_range((void*) l1, PGSIZE);
	flush_dcache_range((void*) l2, PGSIZE);
	dsb_barrier();
	//	asm volatile("mov r1, #1\n\t"
	//                "mcr p15, 0, r1, c3, c0\n\t"
	//                "mov r1, #0x4000\n\t"
	//                "mcr p15, 0, r1, c2, c0\n\t"
	//                "mrc p15, 0, r0, c1, c0, 0\n\t"
	//                "mov r1, #0x00002000\n\t" // 13: Enable High exception vectors
	//                "orr r1, #0x00000004\n\t" // 2:  Enable data cache
	//                "orr r1, #0x00001000\n\t" // 12: Enable instruction cache
	//                "orr r1, #0x00000001\n\t" // 0:  Enable MMU
	//                "orr r0, r1\n\t"
	//                "mcr p15, 0, r0, c1, c0, 0\n\t"
	//                "mov r1, #1\n\t"
	//                "mcr p15, 0, r1, c15, c12, 0\n\t" // Read Performance Monitor Control Register (ARM11)?
	//                ::: "r0", "r1", "cc", "memory");

}

void
mmuinit1(void)
{
	pde_t *l1;
	uint va1, va2;
	uint pa, va;

	l1 = (pde_t*)(K_PDX_BASE);


	// map the rest of RAM after PHYSTART+PHYSIZE
        va = KERNBASE + PHYSIZE;
        for(pa = PHYSTART + PHYSIZE; pa < PHYSTART+pm_size; pa += MBYTE){
	  l1[PDX(va)] = pa|DOMAIN0|PDX_AP(K_RW)|SECTION|CACHED|BUFFERED;
	  //l1[PDX(va)] = pa|DOMAIN0|PDX_AP(K_RW)|SECTION;
	  va += MBYTE;
        }


	// undo identity map of first MB of ram
	l1[PDX(PHYSTART)] = 0;

	// drain write buffer; writeback data cache range [va, va+n]
	va1 = (uint)&l1[PDX(PHYSTART)];
	va2 = va1 + sizeof(pde_t);
	va1 = va1 & ~((uint)CACHELINESIZE-1);
	va2 = va2 & ~((uint)CACHELINESIZE-1);
	#ifdef RPI1
	flush_dcache(va1, va2);
	#else
	// TODO: Thest this just flushing the single entry.
	flush_dcache_range((void*) l1, PGSIZE);
	#endif
 
	// invalidate TLB; DSB barrier used
	flush_tlb();
		
}

