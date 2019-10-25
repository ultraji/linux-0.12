/*
 *  linux/lib/close.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 关闭文件
 * @param[in]	fd		要关闭的文件描述符
 * @retval	
 */
_syscall1(int, close, int, fd)
