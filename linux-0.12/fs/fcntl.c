/*
 *  linux/fs/fcntl.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <string.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <fcntl.h>
#include <sys/stat.h>

extern int sys_close(int fd);

/**
 * 复制文件句柄(文件描述符)
 * @param[in]	fd		欲复制的文件句柄
 * @param[in]	arg		指定新文件句柄的最小数值
 * @retval		成功返回新文件句柄，失败返回出错码
 */
static int dupfd(unsigned int fd, unsigned int arg)
{
	if (fd >= NR_OPEN || !current->filp[fd]) {
		return -EBADF; /* 文件句柄错误 */
	}
	if (arg >= NR_OPEN) {
		return -EINVAL; /* 参数非法 */
	}
	/* 找到一个比arg大的最小的未使用的句柄值 */
	while (arg < NR_OPEN) {
		if (current->filp[arg]) {
			arg++;
		} else {
			break;
		}
	}
	if (arg >= NR_OPEN) {
		return -EMFILE;	/* 打开文件太多 */
	}
	current->close_on_exec &= ~(1<<arg);
	(current->filp[arg] = current->filp[fd])->f_count++;
	return arg;
}

/**
 * 复制文件句柄
 * @param[in]	oldfd	欲复制的指定文件句柄
 * @param[in]	newfd	新文件句柄值(如果newfd已打开，则首先关闭之)
 * @retval		新文件句柄或出错码
 */
int sys_dup2(unsigned int oldfd, unsigned int newfd)
{
	sys_close(newfd);
	return dupfd(oldfd, newfd);
}

/**
 * 复制文件句柄
 * @param[in]	fildes	欲复制的指定文件句柄
 * @retval		新文件句柄(当前最小的未用句柄值)或出错码
 */
int sys_dup(unsigned int fildes)
{
	return dupfd(fildes, 0);
}


/**
 * 文件控制
 * @param[in]	fd		文件句柄
 * @param[in]	cmd		控制命令
 * @param[in]	arg		针对不同的命令有不同的含义
 *						1. F_DUPFD，arg是新文件句可取的最小值
 *						2. F_SETFD，arg是要设置的文件句柄标志
 *						3. F_SETFL，arg是新的文件操作和访问模式
 *						4. F_GETLK、F_SETLK和F_SETLKW，arg是指向flock结构的指针（为
 *						实现文件上锁功能）
 * @retval	若出错，则所有操作都返回 -1;
 *			若成功，那么
 *			1. F_DUPFD，返回新文件句柄
 *			2. F_GETFD，返回文件句柄的当前执行时关闭标志close_on_exec
 *			3. F_GETFL，返回文件操作和访问标志。
 */
int sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{	
	struct file * filp;

	if (fd >= NR_OPEN || !(filp = current->filp[fd])) {
		return -EBADF;
	}
	switch (cmd) {
		case F_DUPFD: /* 复制句柄，返回新文件句柄 */
			return dupfd(fd, arg);
		case F_GETFD: /* 获取文件句柄标志，返回文件句柄的当前执行时关闭标志 */
			return (current->close_on_exec>>fd)&1;
		case F_SETFD: /* 设置文件句柄标志，arg=1设置关闭标志为1，arg=0设置关闭标志为0 */
			if (arg & 1) {
				current->close_on_exec |= (1<<fd);
			} else {
				current->close_on_exec &= ~(1<<fd);
			}
			return 0;
		case F_GETFL: /* 获取文件状态标志和访问模式flag，返回文件操作和访问标志 */
			return filp->f_flags;
		case F_SETFL: /* 设置文件状态标志和访问模式flag */
			filp->f_flags &= ~(O_APPEND | O_NONBLOCK);
			filp->f_flags |= arg & (O_APPEND | O_NONBLOCK);
			return 0;
		case F_GETLK:	case F_SETLK:	case F_SETLKW: /* 未实现 */
			return -1;
		default:
			return -1;
	}
}
