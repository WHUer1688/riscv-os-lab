// 标准输出和报错机制
#include <stdarg.h>
#include "lib/print.h"
#include "lib/lock.h"
#include "dev/uart.h"

volatile int panicked = 0;

static spinlock_t print_lk;

static char digits[] = "0123456789abcdef";

// 函数声明
static void printint(int xx, int base, int sign);
static void printptr(uint64 x);

void print_init(void)
{
    uart_init();
    spinlock_init(&print_lk, "print");
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(const char *fmt, ...)
{
    va_list ap;
    int i, c;
    char *s;

    if (fmt == 0)
        panic("null fmt");

    // 使用自旋锁保护printf，避免多核输出混乱
    spinlock_acquire(&print_lk);
    
    va_start(ap, fmt);
    for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
        if(c != '%'){
            uart_putc_sync(c);
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
                uart_putc_sync(*s);
            break;
        case '%':
            uart_putc_sync('%');
            break;
        default:
            // Print unknown % sequence to draw attention.
            uart_putc_sync('%');
            uart_putc_sync(c);
            break;
        }
    }
    va_end(ap);
    
    spinlock_release(&print_lk);
}

void panic(const char *s)
{
    panicked = 1; // freeze uart output from other CPUs
    for(;;)
        ;
}

void assert(bool condition, const char* warning)
{
    if (!condition) {
        panic(warning);
    }
}

// 打印整数
static void printint(int xx, int base, int sign)
{
    char buf[16];
    int i;
    uint32 x;

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
        uart_putc_sync(buf[i]);
}

// 打印指针
static void printptr(uint64 x)
{
    int i;
    uart_putc_sync('0');
    uart_putc_sync('x');
    for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        uart_putc_sync(digits[x >> (sizeof(uint64) * 8 - 4)]);
}
