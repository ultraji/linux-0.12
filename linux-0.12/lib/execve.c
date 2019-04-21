/*
 *  linux/lib/execve.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 加载并执行子进程(其他程序)
 * int execve(const char *file, char **argv, char **envp)
 * @param[in]	file	被执行程序文件名
 * @param[in]	argv	命令行参数指针数组
 * @param[in]	envp	环境变量指针数组
 * @retval      成功不返回；失败设置出错号，并返回-1
 */
_syscall3(int, execve, const char *, file, char **, argv, char **, envp)
