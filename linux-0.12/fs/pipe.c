/*
 *  linux/fs/pipe.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <signal.h>
#include <errno.h>
#include <termios.h>

#include <linux/sched.h>
#include <linux/mm.h>	/* for get_free_page */
#include <asm/segment.h>
#include <linux/kernel.h>

/**
 * 读管道
 * @param[in]		inode	管道对应的i节点
 * @param[in/out]	buf		用户数据缓冲区指针
 * @param[in]		count	欲读取的字节数
 * @retval			成功返回读取长度，失败返回错误码
 */
int read_pipe(struct m_inode * inode, char * buf, int count)
{
	int chars, size, read = 0;

	while (count > 0) {
		while (!(size = PIPE_SIZE(*inode))) {
			wake_up(& PIPE_WRITE_WAIT(*inode));
			/* 没有写进程，立即返回 */
			if (inode->i_count != 2) { /* are there any writers? */
				return read;
			}
			/* 没有阻塞信号，立即返回 */
			if (current->signal & ~current->blocked) {
				return read ? read : -ERESTARTSYS;
			}
			interruptible_sleep_on(& PIPE_READ_WAIT(*inode));
		}
		/* chars表示当次循环可读取的字节数 */
		chars = PAGE_SIZE - PIPE_TAIL(*inode);
		if (chars > count) {
			chars = count;
		}
		if (chars > size) {
			chars = size;
		}
		count -= chars;
		read += chars;
		size = PIPE_TAIL(*inode);
		PIPE_TAIL(*inode) += chars;
		PIPE_TAIL(*inode) &= (PAGE_SIZE-1);
		while (chars-->0) {
			put_fs_byte(((char *)inode->i_size)[size++], buf++);
		}
	}
	wake_up(& PIPE_WRITE_WAIT(*inode));
	return read;
}


/**
 * 写管道
 * @param[in]	inode		管道对应的i节点
 * @param[in]	buf			数据缓冲区指针
 * @param[in]	count		将写入管道的字节数
 * @retval		成功返回写入的长度，失败返回-1
 */
int write_pipe(struct m_inode * inode, char * buf, int count)
{
	int chars, size, written = 0;

	while (count > 0) {
		while (!(size = (PAGE_SIZE-1) - PIPE_SIZE(*inode))) {
			wake_up(& PIPE_READ_WAIT(*inode));
			/* 没有读进程，发出SIGPIPE信号并立即返回 */
			if (inode->i_count != 2) { /* no readers */
				current->signal |= (1<<(SIGPIPE-1));
				return written ? written : -1;
			}
			sleep_on(& PIPE_WRITE_WAIT(*inode));
		}
		/* chars表示当次循环可写入的字节数 */
		chars = PAGE_SIZE - PIPE_HEAD(*inode);
		if (chars > count) {
			chars = count;
		}
		if (chars > size) {
			chars = size;
		}
		count -= chars;
		written += chars;
		size = PIPE_HEAD(*inode);
		PIPE_HEAD(*inode) += chars;
		PIPE_HEAD(*inode) &= (PAGE_SIZE-1);
		while (chars-->0) {
			((char *)inode->i_size)[size++] = get_fs_byte(buf++);
		}
	}
	wake_up(& PIPE_READ_WAIT(*inode));
	return written;
}


/**
 * 创建管道
 * 在fildes所指的数组中创建一对指向一管道i节点的句柄
 * @param[in/out]	fildes		文件句柄数组：fildes[0]用于读管道，fildes[1]用于写管道
 * @retval		成功返回0，出错返回-1
 */
int sys_pipe(unsigned long * fildes)
{
	struct m_inode * inode;
	struct file * f[2];		/* 文件结构数组 */
	int fd[2];				/* 文件句柄数组 */
	int i, j;

	/* 从系统文件表中取出两个空闲项 */
	j = 0;
	for(i = 0; j < 2 && i < NR_FILE; i ++) {
		if (!file_table[i].f_count) {
			(f[j++] = i + file_table)->f_count++;
		}
	}
	if (j == 1) {
		f[0]->f_count = 0;
	}
	if (j < 2) {
		return -1;
	}
	/* 在当前进程的文件结构指针数组中分配两个文件句柄，用于上面取出的文件表结构项 */
	j = 0;
	for(i = 0; j < 2 && i < NR_OPEN; i ++) {
		if (!current->filp[i]) {
			current->filp[ fd[j] = i ] = f[j];
			j++;
		}
	}
	if (j == 1) {
		current->filp[fd[0]] = NULL;
	}
	if (j < 2) {
		f[0]->f_count = f[1]->f_count = 0;
		return -1;
	}
	/* 申请一个管道使用的i节点 */
	if (!(inode = get_pipe_inode())) {
		current->filp[fd[0]] = current->filp[fd[1]] = NULL;
		f[0]->f_count = f[1]->f_count = 0;
		return -1;
	}
	/* 初始化 */
	f[0]->f_inode = f[1]->f_inode = inode;
	f[0]->f_pos = f[1]->f_pos = 0;
	f[0]->f_mode = 1;		/* read */
	f[1]->f_mode = 2;		/* write */

	put_fs_long(fd[0], 0+fildes);
	put_fs_long(fd[1], 1+fildes);
	return 0;
}

/**
 * 管道io控制函数
 * @param[in]	pino	管道i节点指针
 * @param[in]	cmd		控制命令
 * @param[in]	arg		参数
 * @retval		返回0表示执行成功，否则返回出错码
 */
int pipe_ioctl(struct m_inode *pino, int cmd, int arg)
{
	switch (cmd) {
		/* 取管道中当前可读数据的长度 */
		case FIONREAD:
			verify_area((void *) arg, 4);
			put_fs_long(PIPE_SIZE(*pino), (unsigned long *) arg);
			return 0;
		default:
			return -EINVAL;
	}
}
