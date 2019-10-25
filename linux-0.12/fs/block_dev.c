/*
 *  linux/fs/block_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern int *blk_size[];

/**
 * 数据块写函数
 * 向指定设备（实际先写到高速缓冲中）从给定偏移处写入指定长度数据
 * @param[in]	dev		设备号
 * @param[in]	pos		设备文件中偏移量指针
 * @param[in]	buf		用户空间中缓冲区地址
 * @param[in]	count	要写的字节数
 * @retval		成功返回已写入字节数，若没有写入任何字节或出错则返回出错号
 */
int block_write(int dev, long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS;
	int offset = *pos & (BLOCK_SIZE - 1);
	int chars;
	int written = 0;
	int size;
	struct buffer_head * bh;
	register char * p;

	if (blk_size[MAJOR(dev)]) {
		size = blk_size[MAJOR(dev)][MINOR(dev)];
	} else {
		size = 0x7fffffff; /* 没有对设备指定长度，就使用默认长度2G个块 */
	}
	while (count > 0) {
		if (block >= size) {
			return written ? written : -EIO;
		}
		chars = BLOCK_SIZE - offset; /* 本块可写入的字节数 */
		if (chars > count) {
			chars = count;
		}
		if (chars == BLOCK_SIZE) {
			bh = getblk(dev, block);
		} else {
			bh = breada(dev, block, block+1, block+2, -1);
		}
		block ++;
		if (!bh) {
			return written ? written : -EIO;
		}
		p = offset + bh->b_data;
		offset = 0;
		*pos += chars;
		written += chars;
		count -= chars;
		while (chars-- > 0) {
			*(p++) = get_fs_byte(buf++);
		}
		bh->b_dirt = 1;
		brelse(bh);
	}
	return written;
}


/**
 * 数据块读函数
 * 从指定设备和位置处读入指定长度数据到用户缓冲区中
 * @param[in]	dev		设备号
 * @param[in]	pos		设备文件中领衔量指针
 * @param[in]	buf		用户空间中缓冲区地址
 * @param[in]	count	要传送的字节数
 * @retval		返回已读入字节数。若没有读入任何字节或出错，则返回出错号
 */
int block_read(int dev, unsigned long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS;
	int offset = *pos & (BLOCK_SIZE-1);
	int chars;
	int size;
	int read = 0;
	struct buffer_head * bh;
	register char * p;

	if (blk_size[MAJOR(dev)]) {
		size = blk_size[MAJOR(dev)][MINOR(dev)];
	} else {
		size = 0x7fffffff;
	}
	while (count>0) {
		if (block >= size) {
			return read ? read : -EIO;
		}
		chars = BLOCK_SIZE - offset;
		if (chars > count) {
			chars = count;
		}
		if (!(bh = breada(dev, block, block+1, block+2, -1))) {
			return read ? read : -EIO;
		}
		block++;
		p = offset + bh->b_data;
		offset = 0;
		*pos += chars;
		read += chars;
		count -= chars;
		while (chars-->0) {
			put_fs_byte(*(p++), buf++);
		}
		brelse(bh);
	}
	return read;
}
