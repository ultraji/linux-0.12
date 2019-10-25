/*
 *  linux/fs/read_write.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/segment.h>

#include <unistd.h>		/* import SEEK_SET,SEEK_CUR,SEEK_END */

extern int rw_char(int rw,int dev, char * buf, int count, off_t * pos);
extern int read_pipe(struct m_inode * inode, char * buf, int count);
extern int write_pipe(struct m_inode * inode, char * buf, int count);
extern int block_read(int dev, off_t * pos, char * buf, int count);
extern int block_write(int dev, off_t * pos, char * buf, int count);
extern int file_read(struct m_inode * inode, struct file * filp, char * buf, int count);
extern int file_write(struct m_inode * inode, struct file * filp, char * buf, int count);

/**
 * 重定位文件读写指针 系统调用
 * @param[in]	fd		文件句柄
 * @param[in]	offset	文件读写指针偏移值
 * @param[in]	origin	偏移的起始位置，可有三种选择：SEEK_SET、SEEK_CUR、SEEK_END
 * @retval		成功返回读写偏移值，失败返回失败码
 */
int sys_lseek(unsigned int fd, off_t offset, int origin)
{
	struct file * file;
	int tmp;

	if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(file->f_inode)
	   || !IS_SEEKABLE(MAJOR(file->f_inode->i_dev))) {
		return -EBADF;
	}
	if (file->f_inode->i_pipe) { /* 管道不能操作读写指针 */
		return -ESPIPE;
	}
	/* SEEK_CUR，SEEK_END分支中对相加值判断，既可过滤offset为负数且绝对值比文件长度大的情况，
	 又可以过滤相加超过文件所能支持的最大值(off_t数据类型溢出的情况) */
	switch (origin) {
		case SEEK_SET:	/* 从文件开始处 */
			if (offset < 0) {
				return -EINVAL;
			}
			file->f_pos = offset;
			break;
		case SEEK_CUR:	/* 从当前读写位置 */
			if (file->f_pos + offset < 0) {
				return -EINVAL;
			}
			file->f_pos += offset;
			break;
		case SEEK_END:	/* 从文件尾处 */
			if ((tmp = file->f_inode->i_size + offset) < 0) {
				return -EINVAL;
			}
			file->f_pos = tmp;
			break;
		default:
			return -EINVAL;
	}
	return file->f_pos;
}

/* TODO: 为什么只对读写管道操作判断是否有权限？ */

/**
 * 读文件 系统调用
 * @param[in]	fd		文件句柄
 * @param[in]	buf		缓冲区
 * @param[in]	count	欲读字节数
 * @retval		成功返回读取的长度，失败返回错误码
 */
int sys_read(unsigned int fd, char * buf, int count)
{
	struct file * file;
	struct m_inode * inode;

	if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd])) {
		return -EINVAL;
	}
	if (!count) {
		return 0;
	}
	verify_area(buf, count); 		/* 验证存放数据的缓冲区内存限制 */
	/* 根据文件类型执行相应的读操作 */
	inode = file->f_inode;
	if (inode->i_pipe) { 			/* 管道文件 */
		return (file->f_mode & 1) ? read_pipe(inode, buf, count) : -EIO;
	}
	if (S_ISCHR(inode->i_mode)) { 	/* 字符设备 */
		return rw_char(READ, inode->i_zone[0], buf, count, &file->f_pos);
	}
	if (S_ISBLK(inode->i_mode)) { 	/* 块设备 */
		return block_read(inode->i_zone[0], &file->f_pos, buf, count);
	}
	/* 目录文件或常规文件 */
	if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
		if (count+file->f_pos > inode->i_size) {
			count = inode->i_size - file->f_pos;
		}
		if (count <= 0) {
			return 0;
		}
		return file_read(inode, file, buf, count);
	}
	/* 如果执行到这，说明无法判断文件类型 */
	printk("(Read)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}


/**
 * 写文件 系统调用
 * @param[in]	fd		文件句柄
 * @param[in]	buf		用户缓冲区
 * @param[in]	count	欲写字节数
 * @retval		成功返回写入的长度，失败返回错误码
 */
int sys_write(unsigned int fd, char * buf, int count)
{
	struct file * file;
	struct m_inode * inode;
	
	if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd])) {
		return -EINVAL;
	}
	if (!count) {
		return 0;
	}

	/* 根据文件类型执行相应的写操作 */
	inode = file->f_inode;
	if (inode->i_pipe) { 			/* 管道 */
		/* file->f_mode & 2 即是否有写的权限 */
		return (file->f_mode & 2) ? write_pipe(inode, buf, count) : -EIO;
	}
	if (S_ISCHR(inode->i_mode)) { 	/* 字符设备 */
		return rw_char(WRITE, inode->i_zone[0], buf, count, &file->f_pos);
	}
	if (S_ISBLK(inode->i_mode)) { 	/* 块设备 */
		return block_write(inode->i_zone[0], &file->f_pos, buf, count);
	}
	if (S_ISREG(inode->i_mode)) { 	/* 文件 */
		return file_write(inode, file, buf, count);
	}
	/* 如果执行到这，说明无法判断文件类型 */
	printk("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}
