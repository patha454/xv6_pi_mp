/*****************************************************************
*       uart.c
*       by Zhiyi Huang, hzy@cs.otago.ac.nz
*       University of Otago
*
********************************************************************/



#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "traps.h"
#include "arm.h"

#define GPFSEL0			(MMIO_VA+0x200000)
#define GPFSEL1			(MMIO_VA+0x200004)
#define GPFSEL2			(MMIO_VA+0x200008)
#define GPFSEL3			(MMIO_VA+0x20000C)
#define	GPFSEL4			(MMIO_VA+0x200010)
#define	GPFSEL5			(MMIO_VA+0x200014)
#define GPSET0  		(MMIO_VA+0x20001C)
#define GPSET1			(MMIO_VA+0x200020)
#define GPCLR0  		(MMIO_VA+0x200028)
#define GPCLR1			(MMIO_VA+0x20002C)
#define GPPUD       	(MMIO_VA+0x200094)
#define GPPUDCLK0   	(MMIO_VA+0x200098)
#define GPPUDCLK1		(MMIO_VA+0x20009C)

#define AUX_IRQ			(MMIO_VA+0x215000)
#define AUX_ENABLES     (MMIO_VA+0x215004)
#define AUX_MU_IO_REG   (MMIO_VA+0x215040)
#define AUX_MU_IER_REG  (MMIO_VA+0x215044)
#define AUX_MU_IIR_REG  (MMIO_VA+0x215048)
#define AUX_MU_LCR_REG  (MMIO_VA+0x21504C)
#define AUX_MU_MCR_REG  (MMIO_VA+0x215050)
#define AUX_MU_LSR_REG  (MMIO_VA+0x215054)
#define AUX_MU_MSR_REG  (MMIO_VA+0x215058)
#define AUX_MU_SCRATCH  (MMIO_VA+0x21505C)
#define AUX_MU_CNTL_REG (MMIO_VA+0x215060)
#define AUX_MU_STAT_REG (MMIO_VA+0x215064)
#define AUX_MU_BAUD_REG (MMIO_VA+0x215068)

static uint first_uart_intr;
static uint first_char;
static uint has_first_char;

void led18_on()
{
   setgpiofunc(18, 1); // gpio 18 for Ok Led, set as an output
   setgpioval(18, 1);
}

void led18_off()
{
   setgpiofunc(18, 1); // gpio 18 for Ok Led, set as an output
   setgpioval(18, 0);
}


void
setgpioval(uint pin, uint val)
{
	uint sel, ssel, rsel, shift;

	if(pin > 53) return;
	if(pin >= 32) sel = 1; else sel = 0;
	ssel = GPSET0 + (sel << 2);
	rsel = GPCLR0 + (sel << 2);
	if(sel) shift = (pin - 32) & 0x1f;
	else shift = pin & 0x1f;
	if(val == 0) outw(rsel, 1<<shift);
	else outw(ssel, 1<<shift);
}


void
setgpiofunc(uint pin, uint func)
{
	uint sel, data, shift;

	if(pin > 53) return;
	sel = 0;
	while (pin > 10) {
	    pin = pin - 10;
	    sel++;
	}
	sel = (sel << 2) + GPFSEL0;
	data = inw(sel);
	shift = pin + (pin << 1);
	data &= ~(7 << shift);
	outw(sel, data);
	data |= func << shift;
	outw(sel, data);
}


void
enableirqminiuart(void)
{
        volatile intctrlregs *ip;

        ip = (intctrlregs *)INT_REGS_BASE;
        ip->gpuenable[0] |= (1 << 29);   // enable the miniuart through Aux
}


void
disableirqminiuart(void)
{
        volatile intctrlregs *ip;

        ip = (intctrlregs *)INT_REGS_BASE;
        ip->gpudisable[0] |= (1 << 29);   // disable the miniuart through Aux
}


int uart_disabled;

void 
uartputc(uint c)
{
int i;
volatile int *ptr;

	ptr = &uart_disabled; // need memory barrier here in future when multicore is used
	if(*ptr) return;

	i = 0;
	if(c=='\n') {
		while(1) {
		    if(inw(AUX_MU_LSR_REG) & 0x20) break;
		    i++;
		    if(i>1000) return;
		}
		outw(AUX_MU_IO_REG, 0x0d); // add CR before LF
	}
	i = 0;
	while(1) {
	    if(inw(AUX_MU_LSR_REG) & 0x20) break;
	    i++;
	    if(i>1000) return;
	}
	outw(AUX_MU_IO_REG, c);
}

static int
uartgetc(void)
{
    if(has_first_char) {
	has_first_char = 0;
	return first_char;
    }

    if(inw(AUX_MU_LSR_REG)&0x1) return inw(AUX_MU_IO_REG);
    else return -1;
}

void
miniuartintr(void)
{
uint internal_stat;

  if(uart_disabled) return;

  if(first_uart_intr) {
	first_uart_intr = 0;
	while(inw(AUX_MU_LSR_REG)&0x1) inw(AUX_MU_IO_REG);
	return;
  }

  if(!(inw(AUX_MU_IIR_REG) & 0x4)) return; // if not rx interrupt

  if(!(inw(AUX_MU_STAT_REG) & 0xF0000)) return; // nothing in rx FIFO

  if(inw(AUX_MU_LSR_REG)&0x1) first_char = inw(AUX_MU_IO_REG);
  if(first_char == 0x0) {
	outw(AUX_MU_IER_REG, 0x0);
	uart_disabled = 1;
	return;
  }

  has_first_char = 1;
  consoleintr(uartgetc);
}

void 
uartinit(void)
{
	outw(AUX_ENABLES, 1);
	outw(AUX_MU_CNTL_REG, 0);
	outw(AUX_MU_LCR_REG, 0x3);
	outw(AUX_MU_MCR_REG, 0);
	outw(AUX_MU_IER_REG, 0x1);
	outw(AUX_MU_IIR_REG, 0xC7);
	outw(AUX_MU_BAUD_REG, 270); // (250,000,000/(115200*8))-1 = 270

	setgpiofunc(14, 2); // gpio 14, alt 5
	setgpiofunc(15, 2); // gpio 15, alt 5

	outw(GPPUD, 0);
	delay(10);
	outw(GPPUDCLK0, (1 << 14) | (1 << 15) );
	delay(10);
	outw(GPPUDCLK0, 0);

	outw(AUX_MU_CNTL_REG, 3);

	first_uart_intr = 1;
	uart_disabled = 0;

//	enableirqminiuart();

	//clear tx/rx FIFO
//	outw(AUX_MU_IIR_REG, 0xC6);
	//while(inw(AUX_MU_LSR_REG)&0x1) inw(AUX_MU_IO_REG);

}
