/*
 *  linux/lib/execve.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/** 
 * 加载并执行子进程(其他程序)函数
 * int execve(const char * file, char ** argv, char ** envp)
 * 直接调用了系统中断int 0x80，参数是__NR_execve。参见 include/unistd.h 和 fs/exec.c 程序。
 * @param[in]   file    被执行程序文件名
 * @param[in]   argv    命令行参数指针数组
 * @param[in]   envp    环境变量指针数组
 * @retval  	int
 * 
 */
_syscall3(int, execve, const char *, file, char **, argv, char **, envp)
