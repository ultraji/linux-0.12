/*
 *  linux/lib/dup.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 复制文件描述符
 * @param[in]	fd		需要复制的文件描述符
 * @retval		成功返回新文件句柄，失败返回出错码
 */
_syscall1(int, dup, int, fd)
