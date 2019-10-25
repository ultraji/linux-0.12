/**
 * Linux0.12 调试打印
 */
#include <stdarg.h>

#include <linux/kernel.h>
#include <linux/log_print.h>

extern int vsprintf(char * buf, const char * fmt, va_list args);

static char log_buf[1024];

/* 打印等级功能目前未使用 */
void log_print(unsigned short log_level, const char *fmt, ...) 
{
        va_list args;

        va_start(args, fmt);
        vsprintf(log_buf, fmt, args);
        va_end(args);

        console_print(log_buf);
}
