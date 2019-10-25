/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

/**
 * 复制文件状态信息
 * @param[in]		inode		文件i节点
 * @param[in/out]	statbuf		用户数据空间中stat文件状态结构指针，用于存放取得的状态信息
 * @retval			void
 */
static void cp_stat(struct m_inode * inode, struct stat * statbuf)
{
	struct stat tmp;
	int i;

	verify_area(statbuf, sizeof (struct stat));
	tmp.st_dev		= inode->i_dev;
	tmp.st_ino		= inode->i_num;
	tmp.st_mode		= inode->i_mode;
	tmp.st_nlink	= inode->i_nlinks;
	tmp.st_uid		= inode->i_uid;
	tmp.st_gid		= inode->i_gid;
	tmp.st_rdev		= inode->i_zone[0];	/* 特殊文件的设备号 */
	tmp.st_size		= inode->i_size;
	tmp.st_atime	= inode->i_atime;
	tmp.st_mtime	= inode->i_mtime;
	tmp.st_ctime	= inode->i_ctime;

	for (i = 0; i < sizeof(tmp); i++) {
		put_fs_byte(((char *) &tmp)[i], i + (char *) statbuf);
	}
}


/**
 * 获取文件状态
 * 根据给定的文件名获取相关文件状态信息。
 * @param[in]		filename	指定的文件名
 * @param[in/out]	statbuf		存放状态信息的缓冲区指针
 * @retval			成功返回0，出错返回出错码
 */
int sys_stat(char * filename, struct stat * statbuf)
{
	struct m_inode * inode;

	if (!(inode = namei(filename))) {
		return -ENOENT;
	}
	cp_stat(inode, statbuf);
	iput(inode);
	return 0;
}


/**
 * 获取文件状态 系统调用
 * 根据给定的文件名获取相关文件状态信息。文件路径名中有符号链接文件名，则取符号文件的状态。
 * @param[in]		filename	指定的文件名
 * @param[in/out]	statbuf		存放状态信息的缓冲区指针
 * @retval			成功返回0，出错返回出错码
 */
int sys_lstat(char * filename, struct stat * statbuf)
{
	struct m_inode * inode;

	if (!(inode = lnamei(filename))) {
		return -ENOENT;
	}
	cp_stat(inode,statbuf);
	iput(inode);
	return 0;
}

/**
 * 获取文件状态
 * 根据给定的文件名获取相关文件状态信息。
 * @param[in]		fd			指定文件的句柄
 * @param[in/out]	statbuf		存放状态信息的缓冲区指针
 * @retval			成功返回0，出错返回出错码
 */
int sys_fstat(unsigned int fd, struct stat * statbuf)
{
	struct file * f;
	struct m_inode * inode;

	if (fd >= NR_OPEN || !(f = current->filp[fd]) || !(inode = f->f_inode)) {
		return -EBADF;
	}
	cp_stat(inode,statbuf);
	return 0;
}

/**
 * 符号链接文件 系统调用
 * 该调用读取符号链接文件的内容（即该符号链接所指向文件的路径名字符串），并放到指定长度的用户缓
 * 冲区中。若缓冲区太小，就会截断符号链接的内容。
 * @param[in]		path	符号链接文件路径名
 * @param[in/out]	buf		用户缓冲区
 * @param[in]		bufsiz	缓冲区长度
 * @retval			成功则返回放入缓冲区中的字符数；若失败则返回出错码
*/
int sys_readlink(const char * path, char * buf, int bufsiz)
{
	struct m_inode * inode;
	struct buffer_head * bh;
	int i;
	char c;

	if (bufsiz <= 0) {
		return -EBADF;
	}
	if (bufsiz > 1023) {
		bufsiz = 1023;
	}
	verify_area(buf, bufsiz);
	if (!(inode = lnamei(path))) {
		return -ENOENT;
	}
	if (inode->i_zone[0]) {
		bh = bread(inode->i_dev, inode->i_zone[0]);
	} else {
		bh = NULL;
	}
	iput(inode);
	if (!bh) {
		return 0;
	}
	i = 0;
	while (i < bufsiz && (c = bh->b_data[i])) {
		i++;
		put_fs_byte(c,buf++);
	}
	brelse(bh);
	return i;
}
