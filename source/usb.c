#include <types.h>
#include <defs.h>
#include <uspi.h>
#include <uspios.h>
#include <mailbox.h>
#include <uspi/string.h>
#include <uspienv/timer.h>

#define MAILBOX_FULL  0x80000000
#define MAILBOX_EMPTY 0x40000000

static TTimer USBTimer;
static void KeyPressedHandler (const char *pString);

static char *bufend, *bufptr;

static int usb_strlen(const char *p)
{
int len;
char *s;

	len = 0; s = (char *)p;
	while(*s++ != '\0') len++;
	return len;
}

static int usbgetc(void)
{
char *ptr;

	if(bufptr < bufend) {
	    ptr = bufptr;
	    bufptr ++;
	    return(*ptr);
	} else return -1;
}

static void KeyPressedHandler (const char *pString)
{
int buflen;

	buflen = usb_strlen(pString);
	bufptr = (char *)pString;
	bufend = bufptr + buflen;
	consoleintr(usbgetc);

//    cprintf ("%s", pString);
}

void *malloc (unsigned nSize) {		// result must be 4-byte aligned
	//cprintf("usb: malloc: %d\n", nSize);
	if ( nSize > 4096 ) {
		panic("usb: hit malloc 4K page limit");
	}
	return kalloc();
}

void free (void *pBlock) {
	//cprintf("usb: freeing: %x\n", pBlock);
	kfree(pBlock);
}

void MsDelay (unsigned nMilliSeconds) {
	//cprintf("usb: MsDelay: %d\n", nMilliSeconds);
	//delay (nMilliSeconds * 1000);
	usDelay (nMilliSeconds * 1000);
}

void usDelay (unsigned nMicroSeconds) {
	//cprintf("usb: usDelay: %d\n", nMicroSeconds);
	//delay (nMicroSeconds);
	volatile unsigned * timestamp = (unsigned *) (MMIO_VA+0x003004);
	unsigned stop = * timestamp + nMicroSeconds;
	//cprintf("usb: stamp: %x\n", * timestamp);
	//cprintf("usb: stop: %x\n", stop);
	while ( * timestamp < stop ) {
		__asm__("nop");
	}
}

// returns the timer handle (hTimer)
unsigned StartKernelTimer (unsigned nHzDelay, TKernelTimerHandler *pHandler, void *pParam, void *pContext) {
	//cprintf("usb: Entering StartKernelTimer\n");
	return TimerStartKernelTimer (TimerGet (), nHzDelay, pHandler, pParam, pContext);
	//return 0;
}

void CancelKernelTimer (unsigned hTimer) {
	//cprintf("usb: Entering CancelKernelTimer\n");
	TimerCancelKernelTimer (TimerGet (), hTimer);
}

// returns 0 on failure
int SetPowerStateOn (unsigned nDeviceId) {	// "set power state" to "on", wait until completed
	//cprintf("usb: Entering SetPowerStateOn\n");
	cprintf("usb: Powering DeviceId: %d\n", nDeviceId);
	volatile unsigned int *mailbox = (unsigned int*) MAILBOX_BASE;
	unsigned int result = 0;
	while ( mailbox[6] & MAILBOX_FULL );
	mailbox[8] = 0x80;
	do {
		while ( mailbox[6] & MAILBOX_EMPTY );
	} while ( ( ( result = mailbox[0] ) & 0xf ) != 0 );
	if ( result == 0x80 ) {
		cprintf("usb: Device %d powered on\n", nDeviceId);
		return 1;
	} else {
		cprintf("usb: Failed powering device %d\n", nDeviceId);
		return 0;
	}
}

// returns 0 on failure
int GetMACAddress (unsigned char Buffer[6]) {	// "get board MAC address"
	//cprintf("usb: Entering GetMACAddress\n");
	return 0;
}

void LogWrite (const char *pSource,		// short name of module
	       unsigned	   Severity,		// see above
	       const char *pMessage, ...) {	// uses printf format options
    // variadic function, so we process the message first
    va_list var;
    va_start (var, pMessage);
    TString Message;
    String (&Message);
    StringFormatV (&Message, pMessage, var);
    const char *pString = StringGet (&Message);
    // done processing, ready to print
    cprintf("usb: %s: %s\n", pSource, pString);
    // clean up
    va_end (var);
}

// returns 0 on failure
int usbinit () {
    sti(); // enable interrupts for usb init
    TimerEnvInit( &USBTimer );
    if ( ! TimerInitialize( &USBTimer ) ) {
        cprintf("usb: USB timer init failed\n");
		_Timer ( &USBTimer );
        return 0;
    } else {
        cprintf("usb: USB timer init OK interval=%d\n", CLOCKHZ / HZ);
	}
    if ( ! USPiInitialize() ) {
        cprintf("usb: USB init failed\n");
        cli(); // disable interrupts, will be re-enabled by scheduler
        return 0;
    } else {
        cprintf("usb: USB init OK\n");
        if ( ! USPiKeyboardAvailable() ) {
            cprintf("usb: keyboard not found\n");
        }
        else {
            cprintf("usb: keyboard OK\n");
            USPiKeyboardRegisterKeyPressedHandler (KeyPressedHandler);
        };

//        cli(); // disable interrupts, will be re-enabled by scheduler
        return 1;
    };
}

#ifndef NDEBUG

// display "assertion failed" message and halt
void uspi_assertion_failed (const char *pExpr, const char *pFile, unsigned nLine) {
	cprintf("usb: assertfail: expr: %s: file: %s line: %d\n", pExpr, pFile, nLine);
}

// display hex dump (pSource can be 0)
void DebugHexdump (const void *pBuffer, unsigned nBufLen, const char *pSource /* = 0 */) {
	cprintf("usb: hexdump: source: %s\n", pSource); //TODO
}

#endif
