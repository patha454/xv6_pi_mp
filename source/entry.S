/*******************************************************************
*	entry.s
*	by Zhiyi Huang, hzy@cs.otago.ac.nz
*	University of Otago
*
********************************************************************/

.section .init, "ax"

.equ VA_OFFSET, 0x80000000
	
.globl _start
_start:
	b boot_reset
	b boot_sleep	// undefined
	b boot_sleep	// svc
	b boot_sleep	// prefetch
	b boot_sleep	// abort
	b boot_sleep	// hypervisor
	b boot_sleep	// irq
	b boot_sleep	// fiq

	.balign 4
boot_sleep:
	// Note core error in shared memory
	mcr 	p15, 0, r0, c0, c0, 5
	ubfx	r0, r0, #0, #8
	lsl 	r0, #2
	ldr 	r1, =cpu_sig
	ldr 	r2, =VA_OFFSET
	sub 	r1, r1, r2
	mov 	r2, #0xff                   //We use 0xFF as the early boot error code.
	str	r2, [r1, r0]
	wfe
	b boot_sleep

boot_reset:
	
	// Switch to SVC mode, all interrupts disabled
	.set PSR_MODE_SVC, 0x13
	.set PSR_MODE_IRQ_DISABLED, (1<<7)
	.set PSR_MODE_FIQ_DISABLED, (1<<6)
	msr	cpsr_c, #(PSR_MODE_SVC + PSR_MODE_FIQ_DISABLED + PSR_MODE_IRQ_DISABLED)

	//Note which cores have started.
	//mrc  	p15, 0, r0, c0, c0, 5	//Read MP affinity register
	//ubfx 	r0, r0, #0, #8	        //Extract core ID
	//lsl  	r0, #2			//Multiply by 4 to convert onto a byte offset.
	//ldr   	r1, =cpu_sig		//Load CPU_SIGNAL VA
	//ldr 	r2, =VA_OFFSET          //Load the VA/PA offset
	//sub 	r1, r1, r2		//Convert CPU_SIGnAL VA to PA.
	//mov	r2, #1			//Move CPU_STARTED signal into r2 - PC test actual addr
	//str     r2, [r1, r0]            //Store CPU_STARTED signal in the core's CPU_SIGNAL slot.
	
	// Disable caches, MMU, and flow prediction
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(0x1 << 12)	// Disable instruction cache
	bic	r0, r0, #(0x1 << 11)	// Disable flow prediction
	bic	r0, r0, #(0x1 <<  2)	// Disable data cache
	bic	r0, r0, #0x1		// Disable MMU
	mcr	p15, 0, r0, c1, c0, 0

	// Enable ACTLR.SMP bit
	mrc	p15, 0, r0, c1, c0, 1
	orr	r0, r0, #(1 << 6)
	mcr	p15, 0, r0, c1, c0, 1

	// Or the ECTLR.SMPEN bit on the A53.
	mrrc    p15, 1, r0, r1, c15
	orr     r0, #1 << 6
	orr     r1, #1 << 6
	mcrr    p15, 1, r0, r1, c15

	// Invalidate TLB and branch prediction caches.
	mov	r0,#0
	mcr	p15, 0, r0, c8, c7, 0	// Invalidate unified TLB
	mcr	p15, 0, r0, c7, c5, 6	// Invalidate BPIALL

	// Update ARM vector address (early binding for debug)
	ldr	r0, =_start
	mcr	p15, 0, r0, c12, c0, 0	// VBAR

	// Cache invalidation for older Cortex-A
	// Note: Cortex-A7 (RPI2) does not need this part.
	// Invalidate l1 instruction cache
	mrc p15, 1, r0, c0, c0, 1
	tst r0, #0x3
	mov r0, #0
	mcrne p15, 0, r0, c7, c5, 0

	// Invalidate data/unified caches
	mrc p15, 1, r0, c0, c0, 1
	ands r3, r0, #0x07000000
	mov r3, r3, lsr #23
	beq finished

	mov r10, #0
loop1:
	add r2, r10, r10, lsr #1
	mov r1, r0, lsr r2
	and r1, r1, #7
	cmp r1, #2
	blt skip

	mcr p15, 2, r10, c0, c0, 0
	isb
	mrc p15, 1, r1, c0, c0, 0
	and r2, r1, #7
	add r2, r2, #4
	ldr r4, =0x3ff
	ands r4, r4, r1, lsr #3
	clz r5, r4
	ldr r7, =0x7fff
	ands r7, r7, r1, lsr #13
loop2:
	mov r9, r4

loop3:
	orr r11, r10, r9, lsl r5
	orr r11, r11, r7, lsl r2
	mcr p15, 0, r11, c7, c6,2
	subs r9, r9, #1
	bge loop3
	subs r7, r7, #1
	bge loop2

skip:
	add r10, r10, #2
	cmp r3, r10
	bgt loop1
finished:
	dsb

	// MMU configurations
	// Activate TTBR0 by TTBCR reg
	mov	r0,#0x0
	mcr	p15, 0, r0, c2, c0, 2

	// Set master translation table address (TTBR0)
	ldr	r0,=K_PDX_BASE
	mov	r1, #0x08
	orr	r1,r1,#0x40
	orr	r0,r0,r1
	mcr	p15, 0, r0, c2, c0, 0

	// Set depricated ARM domains
	mrc	p15, 0, r0, c3, c0, 0
	ldr	r0, =0x55555555
	mcr	p15, 0, r0, c3, c0, 0

	// Set all CPUs to wait except the primary CPU
	mrc p15, 0, r0, c0, c0, 5
	ubfx r0, r0, #0, #8
	cmp r0, #0
	
	wfene
	bne mp_continue
	
	// MMU Phase 1
	// Create master translation table (page directory index)
mmu_phase1:

	ldr	r0,=K_PDX_BASE
	ldr	r1,=0xfff
	ldr	r2,=0

pagetable_invalidate:
	str	r2, [r0, r1, lsl#2]
	subs r1, r1, #1
	bpl	pagetable_invalidate

	// Page table attribute
	// 0x14406= 0b0010 100 01 0 0000 0 01 10  Outer disabled inner write back
	// 0x14c06= 0b0010 100 11 0 0000 0 01 10
	// 0x15c06= 0b0010 101 11 0 0000 0 01 10
	// 0x1140e= 0b0010 001 01 0 0000 0 11 10  Inner & outer write back
	//    140e= 0b0000 001 01 0 0000 0 11 10  Inner & outer write back not shearable
	// 0x10408= 0b0010 000 01 0 0000 0 10 00  Inner & outer write though
        // 0x16406= 0b0010-110 01 0 0000 0 01 10  Outer write through inner write back works
	//            ZGSA-TEX-AP-I-DOMN-X-CB-10
        //               2     10
	//ldr	r2,=0x14c06	//Inner cache
	//ldr	r2,=0x15c06 	//Outer cache
	//	ldr	r2,=0x14406    // 1 MiB page, bufferable, read/write at any privl, cacheable TEX (but not cached.)
	//ldr r2, =0x1140e
	ldr r2, =0x1140e
	//ldr     r2,=0x15c0e
	// Map __pa_init_start to __pa_init_start address
	ldr	r1,=PHYSTART
	lsr	r1, #20
	orr	r3, r2, r1, lsl#20	
	str	r3, [r0, r1, lsl#2]

	//r0 is the page table base.
	// Map __va_kernel_start to __pa_init_start address
	ldr	r1,=PHYSTART		// Load the physical address to map into r1
	lsr	r1, #20			// Truncate to 1 MiB precision
	orr	r3, r2, r1, lsl#20      // Set the properties for the page; store in r3.
	ldr	r1,=KERNBASE		// Load the virtual address to map from.
	lsr	r1, #20			// Truncate to 1 MiB precision
	str	r3, [r0, r1, lsl#2]	// Store the entry in PDX(r1 << 2)

	
	// Map the rest of the physical memory into kernel space.
	// For now, this only maps 256 MiB - RPI2B has 947 avalible.
	mov r5, #1			// Index of the page to be mapped
map_kernel_pages:	
	lsl r6, r5, #20			// Offset of the page to map.
	ldr r1, =PHYSTART		// Load the base of the physical memory
	add r1, r1, r6			// Compute physical address of the page to map.
	lsr r1, #20			// Truncate to 1 MiB precision
	orr r3, r2, r1, lsl#20		// Set the properties for the page; store in r3.
	ldr r1, =KERNBASE		// Load the base of the virtual address range.
	add r1, r1, r6			// Compute the virtual address to map from.
	lsr r1, #20			// Truncate to 1 MiB precision.
	str r3, [r0, r1, lsl#2]		// Store the entry in PDX(r1 << 2)
	ldr r7, =PHYSIZE		// Load the physical size to map
	lsr r7, #20			// Break into mb
	sub r7, r7, #1
	cmp r5, r7 		      	// Test if we've mapped in all avalible memory.
	addne r5, r5, #1		// Increment the page index.
	bne map_kernel_pages		// Map the next page
	clrex
	
	
	// Map device MMIO (just GPIO for LED debug)
	ldr	r2,=0xc16	//device template
	ldr	r1,=(MMIO_PA+0x200000)
	lsr	r1, #20
	orr	r3, r2, r1, lsl#20
	ldr	r1,=(MMIO_VA+0x200000)
	lsr	r1, #20
	str	r3, [r0, r1, lsl#2]

	// All processors will start from here after waiting:
mp_continue:
	// Set up 4k per core system tacks.

	mrc 	p15, 0, r0, c0, c0, 5          // Store CPU id in r0
	ubfx 	r0, r0, #0, #8
	add     r1, r0, #1                     // Compute stack offset in r1
	lsl     r1, #11
	ldr     r2, =KERNBASE                  // Compute stack address in r1
	add     r1, r1, r2
	mov 	sp, r1		              // Set the stack pointer
	
	// Enable I/D$, MMU, and flow prediction.
	dsb
	ldr r1,=_pagingstart

	//Clear ICache and Dcache on Cortex A7 (ARMv7)
	#ifdef RPI1
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4 /* dsb */
	mov r0, #0
	mcr p15, 0, r0, c7, c14, 0 /* invalidate d-cache */
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0 /* invalidate i-cache */
	#else
	dsb
	isb
	//Cortex A53 (ARMv8) flush I Cache
        mrc p15, 1, r0, c0, c0, 1
	tst r0, #0x3
	mov r0, #0
	mcrne p15, 0, r0, c7, c5, 0

	//Cortex A53 (ARMv*) invalidate D cache
	mov r0, #0
	mcr p15, 2, r0, c0, c0, 0
	mrc p15, 1, r0, c0, c0, 0

	movw r1, #0x7fff
	and r2, r1, r0, lsr #13

	movw r1, #0x3ff

	and r3, r1, r0, lsr #3
	and r2, r2, #1

	and r0, r0, #0x7
	and r0, r0, #4

	clz r1, r3
	add r4, r3, #1
1:	sub r2, r2, #1
	mov r3, r4
2:	subs r3, r3, #1
	mov r5, r3, lsl r1
	mov r6, r2, lsl r0
	orr r5, r5, r6
	mcr p15, 0, r5, c7, c6, 2
	bgt 2b
	cmp r2, #0
	bgt 1b
	dsb
	isb
	
	#endif

	//Cortex A53 ARMv8 flush D cache.
	
	// The I cache and D cache should be invalidated before enabeling cacheing.

	mrc	p15, 0, r0, c1, c0, 0
	orr r0, r0,	#(0x1 << 13)	// High vector
	orr	r0, r0, #(0x1 << 12)	// Enable I$
	orr	r0, r0, #(0x1 << 11)	// Enable flow prediction
	orr	r0, r0, #(0x1 <<  2)	// Enable D$
	orr	r0, r0, #0x1	        // Enable MMU
	bic     r0, r0, #(0x1 << 28)    // Disable TEX remap.
	mcr	p15, 0, r0, c1, c0, 0

	mrc  	p15, 0, r0, c0, c0, 5	//Read MP affinity register
	ubfx 	r0, r0, #0, #8	        //Extract core ID
	lsl  	r0, #2			//Multiply by 4 to convert onto a byte offset.
	ldr     r1, =cpu_sig		//Load CPU_SIGNAL VA
	ldr 	r2, =VA_OFFSET          //Load the VA/PA offset
	sub 	r1, r1, r2		//Convert CPU_SIGnAL VA to PA.
	mov	r2, #1			//Move CPU_STARTED signal into r2 - PC test actual addr
	str     r2, [r1, r0]            //Store CPU_STARTED signal in the core's CPU_SIGNAL slot.

	ldr r1, =_pagingstart
	bx r1

	.section .text
.global _pagingstart
_pagingstart:
        mrc p15, 0, r5, c0, c0, 5	// Read MPIDR into r5
	ubfx r5, r5, #0, #8		// Extract CPU ID.
	ands r5, r5, #0x3               // Test if this is the master CPU.
	bleq cmain  /* Init OS with the primary CPU */
	bl aux_main /* Init secondary CPUs. */               
	bl NotOkLoop

.global acknowledge
acknowledge:
	//Turn on the LED
	ldr r2,=MMIO_VA
	add r2,r2,#0x200000
	//Function select
	mov r3,#1

	#ifdef RPI1
	lsl r3,#18			//Pi1 ACT LED: GPIO#16 (GPFSEL1)
	str r3,[r2,#0x4]
	mov r3,#1
	lsl r3,#16
	str r3,[r2,#0x28]	//Pi1 (GPCLR0)
	#endif

	#ifdef RPI2
	lsl r3,#21			//Pi2 ACT LED: GPIO#47 (GPFSEL4)
	str r3,[r2,#0x10]
	mov r3,#1
	lsl r3,#15
	str r3,[r2,#0x20] //Pi2 (GPSET1)
	#endif

	bx lr

.global dsb_barrier
dsb_barrier:
	#ifdef RPI1
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4
	#else
	dsb
	isb
	#endif
	bx lr
.global flush_dcache_all
flush_dcache_all:
	#ifdef RPI1
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4 /* dsb */
	mov r0, #0
	mcr p15, 0, r0, c7, c14, 0 /* invalidate d-cache */
	#else
	dsb
	isb
	#endif
	bx lr
.global flush_idcache	
flush_idcache:
	#ifdef RPI1
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 4 /* dsb */
	mov r0, #0
	mcr p15, 0, r0, c7, c14, 0 /* invalidate d-cache */
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0 /* invalidate i-cache */
	#else
	// RPI2/3 has no command to flush the entire data cache, so this only
	// flushes the instruction cache.
	// Use flush_dcache_range or invalidate_dcache_range to maintain
	// sections of the data cache by virtual address.
	dsb
	isb
	//Cortex A53 (ARMv8) flush I Cache only.
	mrc p15, 1, r0, c0, c0, 1
	tst r0, #0x3
	mov r0, #0
	mcrne p15, 0, r0, c7, c5, 0
	//I Cache flushed.
	//The entire D cache can not be flushed in ARMv8.
	#endif
	bx lr
.global flush_tlb
flush_tlb:
	#ifdef RPI1
	mov r0, #0
	mcr p15, 0, r0, c8, c7, 0
	mcr p15, 0, r0, c7, c10, 4
	#else
	dsb
	isb
	mov	r0,#0
	mcr     p15, 0, r0, c8, c6, 0   // Invalidate L1 data TLB.
	mcr     p15, 0, r0, c8, c5, 0   // Invalidate L1 instruction TLB.
	mcr	p15, 0, r0, c8, c7, 0	// Invalidate unified (L2) TLB
	mcr     p15, 0, r0, c8, c3, 0   // Invalidate entire TLB inner shearalbe
	mcr	p15, 0, r0, c7, c5, 6	// Invalidate BPIALL
	dsb
	isb
	#endif
	bx lr
.global flush_dcache /* flush a range of data cache flush_dcache(va1, va2) */
flush_dcache:
	#ifdef RPI1
	mcrr p15, 0, r0, r1, c14, 0
	#else
	dsb
	isb
	#endif
	bx lr

.global set_pgtbase /* set the page table base set_pgtbase(base) */
set_pgtbase:
	mcr p15, 0, r0, c2, c0
	bx lr

.global getsystemtime
getsystemtime:
	ldr r0, =(MMIO_VA+0x003004) /* addr of the time-stamp lower 32 bits */
	ldrd r0, r1, [r0]
	bx lr

.global cpu_sig
cpu_sig:	
core0:	.word  0x0
core1:	.word  0x0
core2:	.word  0x0
core3:	.word  0x0

.section .data

.align 4
.globl font
font:
	.incbin "font1.bin"



.align 4
.global _binary_initcode_start
_binary_initcode_start:
	.incbin "initcode"
.global _binary_initcode_end
_binary_initcode_end:

.align 4
.global _binary_fs_img_start
_binary_fs_img_start:
        .incbin "fs.img"
.global _binary_fs_img_end
_binary_fs_img_end:
