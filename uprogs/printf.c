#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

#define BUFFERSIZE 512
struct buffer {
  char data[BUFFERSIZE];
  uint length;
};

struct buffer writebuffer[NOFILE];

static void
putc(int fd, char c)
{
  if (fd >= NOFILE) {
    return;
  }
  writebuffer[fd].data[writebuffer[fd].length++] = c;
  if (c == '\0' || c == '\n'|| writebuffer[fd].length == BUFFERSIZE) {
    write(fd, &writebuffer[fd].data, writebuffer[fd].length);
    writebuffer[fd].length = 0;
  }
  // write(fd, &c, 1);
}

static void flushbuffer(int fd) {
  if (writebuffer[fd].length > 0) {
    write(fd, &writebuffer[fd].data, writebuffer[fd].length);
    writebuffer[fd].length = 0;
  }
}

u32 div(u32 n, u32 d)  // long division
{
    u32 q=0, r=0;
    int i;

    for(i=31;i>=0;i--){
        r = r << 1;
        r = r | ((n >> i) & 1);
        if(r >= d) {
            r = r - d;
            q = q | (1 << i);
        }
    }
    return q;
}

static void
printint(int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x, y, b;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  b = base;
  i = 0;
  do{
    y = div(x, b);
    buf[i++] = digits[x - y * b];
  }while((x = y) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(fd, buf[i]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
printf(int fd, char *fmt, ...)
{
  char *s;
  int c, i, state;
  uint *ap;
  writebuffer[fd].length = 0;
  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, *ap, 10, 1);
        ap++;
      } else if(c == 'x' || c == 'p'){
        printint(fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(fd, *ap);
        ap++;
      } else if(c == '%'){
        putc(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(fd, '%');
        putc(fd, c);
      }
      state = 0;
    }
  }
  flushbuffer(fd);
}
