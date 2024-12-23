//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c, locking;
  char *s;

  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&pr.lock);
}

void
panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}

// |-----------------------------| <- 栈顶 (sp，刚开始调用函数时)
// | 调用者的返回地址 (ra)        | <- 保存调用函数后需要跳转的返回地址
// |-----------------------------|
// | 调用者的帧指针 (s0/fp)       | <- 保存调用者的帧指针
// |-----------------------------|
// | 函数的参数 (多余的部分)       | <- 当寄存器不足以存储所有参数时，剩余的参数存放在这里
// |-----------------------------|
// | 临时变量/局部变量            | <- 函数内部的局部变量（自动分配的空间）
// |-----------------------------|
// | 调用者保存的寄存器           | <- 需要保存的调用者寄存器值（被调用函数会覆盖它们）
// |-----------------------------|
// | 临时空间（动态分配的内存）    | <- 用于动态内存分配或其他操作所需的额外空间
// |-----------------------------| <- 栈底 (sp，当前帧的最底部)


void
backtrace(void)
{
  // 1. 获取当前的栈顶指针 fp
  uint64 fp = r_fp();
  // 2. 向下对齐到当前存储栈页面的底部 bottom 
  uint64 bottom = PGROUNDDOWN(fp);
  
  printf("backtrace:\n");
  // 3. 遍历整个栈页面
  while (fp > bottom) {
    uint64 ra = *(uint64*)(fp - 8);
    printf("%p\n", ra);
    fp = *(uint64*)(fp - 16);
  }
}
