/*
 *  linux/lib/wait.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <sys/wait.h>

/** 
 * 等待进程终止 系统调用
 * pid_t waitpid(pid_t pid, int * wait_stat, int options)
 * @param[in]	pid			等待被终止进程的进程id，或者是用于指定特殊情况的其他特定数值
 * @param[in]	wait_stat	用于存放状态信息
 * @param[in]	options		WNOHANG 或 WUNTRACED 或 0
 * @retval		成功返回已经结束的子进程的进程号，失败返回-1
 */
_syscall3(pid_t, waitpid, pid_t, pid, int *, wait_stat, int, options)


/** 
 * wait() 系统调用
 * 直接调用waitpid()函数
 * @param[in]	wait_stat	用于存放状态信息
 * @retval  	成功返回已经结束的子进程的进程号，失败返回-1
 */
pid_t wait(int *wait_stat)
{
	return waitpid(-1, wait_stat, 0);
}
