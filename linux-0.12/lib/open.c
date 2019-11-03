/*
 *  linux/lib/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

/** 
 * 打开(有可能创建)一个文件
 * @param[in]	filname	文件名
 * @param[in]	flag	文件打开标志
 * @param[in]	...		文件属性
 * @retval  	成功返回文件描述符，出错返回-1并且设置出错号
 */
int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

	va_start(arg, flag);
	__asm__(
		"int $0x80"
		:"=a" (res)
		:"0" (__NR_open), "b" (filename), "c" (flag), "d" (va_arg(arg, int))
	);
	if (res >= 0){
		return res;
	}
	errno = -res;
	return -1;
}
