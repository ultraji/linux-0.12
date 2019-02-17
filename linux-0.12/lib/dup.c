/*
 *  linux/lib/dup.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 复制文件描述符函数
 * int dup(int fd)
 * 直接调用了系统中断int 0x80，参数是__NR_dup。
 * @param[in]   fd      文件描述符
 * @retval  	int
 * 
 */
_syscall1(int, dup, int, fd)
