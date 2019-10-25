/*
 *  linux/fs/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>

#include <asm/segment.h>

/**
 * 取文件系统信息(未实现)
 * 该系统调用用于返回已安装（mounted）文件系统的统计信息。
 * @param[in]	dev		含有用户已安装文件系统的设备号
 * @param[in]	ubuf	一个ustat结构缓冲区指针，用于存放系统返回的文件系统信息
 * @retval		成功时返回0，并且ubuf指向的ustate结构被添入文件系统总空闲块和空闲i节点数
 */
int sys_ustat(int dev, struct ustat * ubuf)
{
	return -ENOSYS;
}


/**
 * 设置文件访问和修改时间
 * @param[in]	filename	文件名
 * @param[in]	times		访问和修改时间结构指针
 * 							1. times!=NULL，则取utimbuf结构中的时间信息来设置文件的访问
 							  和修改时间
 * 							2. times==NULL，则取系统当前时间来设置指定文件的访问和修改时间域
 * @retval		成功返回0，失败返回错误码
 */
int sys_utime(char * filename, struct utimbuf * times)
{
	struct m_inode * inode;
	long actime, modtime;

	if (!(inode = namei(filename))) {
		return -ENOENT;
	}
	if (times) {
		actime = get_fs_long((unsigned long *) &times->actime);
		modtime = get_fs_long((unsigned long *) &times->modtime);
	} else {
		actime = modtime = CURRENT_TIME;
	}
	inode->i_atime = actime;
	inode->i_mtime = modtime;
	inode->i_dirt = 1;
	iput(inode);
	return 0;
}

/*
 * XXX should we use the real or effective uid?  BSD uses the real uid,
 * so as to make this call useful to setuid programs.
 */
/*
 * XXX我们该用真实用户id（ruid）还是有效有户id（euid）？BSD系统使用了真实用户id，以使该调用可以
 * 供setuid程序使用。
 * （注：POSIX标准建议使用真实用户ID）
 * （注1：英文注释开始的‘XXX’表示重要提示）
 */

/**
 * 检查文件的访问权限
 * @param[in]	filename		文件名
 * @param[in]	mode			检查的访问属性
 * @retval		如果访问允许的话，则返回0；否则，返回出错码
 */
int sys_access(const char * filename, int mode)
{
	struct m_inode * inode;
	int res, i_mode;

	mode &= 0007;
	if (!(inode = namei(filename))) {
		return -EACCES;
	}
	i_mode = res = inode->i_mode & 0777;
	iput(inode);
	if (current->uid == inode->i_uid) {
		res >>= 6;
	}
	else if (current->gid == inode->i_gid) {
		res >>= 6;
	}
	if ((res & 0007 & mode) == mode) {
		return 0;
	}
	/*
	 * XXX we are doing this test last because we really should be
	 * swapping the effective with the real user id (temporarily),
	 * and then calling suser() routine.  If we do call the
	 * suser() routine, it needs to be called last. 
	 */
	if ((!current->uid) &&
	    (!(mode & 1) || (i_mode & 0111)))
		return 0;
	return -EACCES;
}


/**
 * 改变当前工作目录 系统调用
 * @param[in]	filename	目录名
 * @retval		成功则返回0，否则返回出错码
 */
int sys_chdir(const char * filename)
{
	struct m_inode * inode;

	if (!(inode = namei(filename)))
		return -ENOENT;
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}
	iput(current->pwd);
	current->pwd = inode;
	return (0);
}


/**
 * 改变根目录 系统调用
 * 把指定的目录名设置成为当前进程的根目录“/”
 * @param[in]	filename	文件名
 * @retval		成功返回0，否则，返回出错码
 */
int sys_chroot(const char * filename)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}
	iput(current->root);
	current->root = inode;
	return (0);
}


/**
 * 修改文件属性 系统调用
 * @param[in]	filename	文件名
 * @param[in]	mode		新的文件属性
 * @retval		成功返回0，失败返回出错码
 */
int sys_chmod(const char * filename,int mode)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if ((current->euid != inode->i_uid) && !suser()) {
		iput(inode);
		return -EACCES;
	}
	inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
	inode->i_dirt = 1;
	iput(inode);
	return 0;
}

/**
 * 修改文件宿主 系统调用
 * @param[in]	filename	文件名
 * @param[in]	uid			用户ID
 * @param[in]	gid			组ID
 * @retval		成功则返回0，失败返回出错码
 */
int sys_chown(const char * filename,int uid,int gid)
{
	struct m_inode * inode;

	if (!(inode = namei(filename))) {
		return -ENOENT;
	}
	if (!suser()) {
		iput(inode);
		return -EACCES;
	}
	inode->i_uid=uid;
	inode->i_gid=gid;
	inode->i_dirt=1;
	iput(inode);
	return 0;
}


/**
 * 检查字符设备类型
 * 该函数仅用于下面文件打开系统调用sys_open()，用于检查若打开的文件是tty终端字符设备时，需要对
 * 当前进程的设置和对tty表的设置。
 * @param[in]	inode		i节点
 * @param[in]	dev			设备号
 * @param[in]	flag		文件操作标志
 * @retval		成功返回0，失败返回-1(对应字符设备不能打开)
 */
static int check_char_dev(struct m_inode * inode, int dev, int flag)
{
	struct tty_struct *tty;
	int min;

	if (MAJOR(dev) == 4 || MAJOR(dev) == 5) {
		if (MAJOR(dev) == 5) {
			min = current->tty;
		} else {
			min = MINOR(dev);
		}
		if (min < 0) {
			return -1;
		}
		if ((IS_A_PTY_MASTER(min)) && (inode->i_count>1)) {
			return -1;
		}
		tty = TTY_TABLE(min);
		if (!(flag & O_NOCTTY) &&
		    current->leader &&
		    current->tty<0 &&
		    tty->session==0) {
			current->tty = min;
			tty->session= current->session;
			tty->pgrp = current->pgrp;
		}
		if (flag & O_NONBLOCK) {
			TTY_TABLE(min)->termios.c_cc[VMIN] =0;
			TTY_TABLE(min)->termios.c_cc[VTIME] =0;
			TTY_TABLE(min)->termios.c_lflag &= ~ICANON;
		}
	}
	return 0;
}


/**
 * 打开(或创建)文件
 * @note 实际上open的操作是将进程中的文件描述符指向了系统中的文件表项，该项又指向了打开的文件
 * 		索引节点(inode)。
 * @param[in]	filename	文件名
 * @param[in]	flag		打开文件标志
 * @param[in]	mode		文件属性
 * @retval		成功返回文件句柄，失败返回出错码
 */
int sys_open(const char * filename, int flag, int mode)
{
	struct m_inode * inode;
	struct file * f;
	int i, fd;

	mode &= 0777 & ~current->umask;

	/* 在用户文件描述符表找到最小的文件描述符fd */
	for(fd = 0; fd < NR_OPEN; fd ++) {
		if (!current->filp[fd]) {
			break;
		}
	}
	if (fd >= NR_OPEN) {
		return -EINVAL;
	}
	current->close_on_exec &= ~(1<<fd);

	/* 在文件表中找到空闲的文件结构项 */
	f = 0 + file_table;
	for (i = 0; i < NR_FILE; i++, f++) {
		if (!f->f_count) {
			break;
		}
	}
	if (i >= NR_FILE) {
		return -EINVAL;
	}
	(current->filp[fd] = f)->f_count++;
	/* 在内存的索引节点表中找到文件对应的i节点 */
	if ((i = open_namei(filename, flag, mode, &inode)) < 0) {
		current->filp[fd] = NULL;
		f->f_count = 0;
		return i;
	}
/* ttys are somewhat special (ttyxx major==4, tty major==5) */
	if (S_ISCHR(inode->i_mode)) {
		if (check_char_dev(inode, inode->i_zone[0], flag)) {
			iput(inode);
			current->filp[fd] = NULL;
			f->f_count = 0;
			return -EAGAIN;
		}
	}
/* Likewise with block-devices: check for floppy_change */
	if (S_ISBLK(inode->i_mode)) {
		check_disk_change(inode->i_zone[0]);
	}
	f->f_mode = inode->i_mode;
	f->f_flags = flag;
	f->f_count = 1;
	f->f_inode = inode;
	f->f_pos = 0;
	return (fd);
}


/**
 * 创建文件
 * @param[in]	pathname	路径名
 * @param[in]	mode		文件属性
 * @retval		成功返回文件句柄，失败返回出错码
 */
int sys_creat(const char * pathname, int mode)
{
	return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}


/**
 * 关闭文件
 * @param[in]	fd		文件句柄
 * @retval		成功返回0，失败返回出错码
 */
int sys_close(unsigned int fd)
{	
	struct file * filp;

	if (fd >= NR_OPEN) {
		return -EINVAL;
	}
	current->close_on_exec &= ~(1<<fd);
	if (!(filp = current->filp[fd])) {
		return -EINVAL;
	}
	current->filp[fd] = NULL;
	if (filp->f_count == 0) {
		panic("Close: file count is 0");
	}
	if (--filp->f_count) {
		return (0);
	}
	iput(filp->f_inode);
	return (0);
}
