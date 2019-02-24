/*
 *  linux/lib/close.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 关闭文件
 * int close(int fd)
 * 直接调用了系统中断int 0x80，参数是__NR_close。
 * @param[in]   fd      文件描述符
 * @retval  	int
 * 
 */
_syscall1(int, close, int, fd)
