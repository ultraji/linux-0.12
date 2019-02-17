/*
 *  linux/lib/_exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 内核使用的程序(退出)终止函数
 * 直接调用系统中断int 0x80，功能号__NR_exit
 * @param[in]   exit_code	退出码 
 * @retval  	void
 * 
 */ 
volatile void _exit(int exit_code)
{
	__asm__("int $0x80"::"a" (__NR_exit), "b" (exit_code));
}
