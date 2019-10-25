/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */
#include <linux/sched.h>
#include <sys/stat.h>

/** 
 * 释放所有一次间接块
 * @param[in]	dev		文件系统所有设备的设备号
 * @param[in]	block	逻辑块号
 * @retval  	成功返回1，失败返回0
 */
static int free_ind(int dev, int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;
	int block_busy;

	/* 如果逻辑块号为0，则返回 */
	if (!block) {
		return 1;
	}
	block_busy = 0;
	/* 读取一次间接块，并释放其上表明使用的所有逻辑块，然后释放该一次间接块的缓冲块 */
	if ((bh = bread(dev, block))) {
		p = (unsigned short *) bh->b_data;	/* 指向缓冲块数据区 */
		for (i = 0; i < 512; i++, p++) {	/* 每个逻辑块上可有512个块号 */
			if (*p) {
				if (free_block(dev, *p)) {	/* 释放指定的设备逻辑块 */
					*p = 0;                 /* 清零 */
					bh->b_dirt = 1;			/* 设置已修改标志 */
				} else {
					block_busy = 1;			/* 设置逻辑块没有释放标志 */
				}
			}
		}
		brelse(bh);							/* 然后释放间接块占用的缓冲块 */
	}
	/* 最后释放设备上的一次间接块。但如果其中有逻辑块没有被释放，则返回0(失败) */
	if (block_busy) {
		return 0;
	} else {
		return free_block(dev, block);		/* 成功则返回1，否则返回0 */
	}
}


/** 
 * 释放所有二次间接块
 * @param[in]	dev		文件系统所有设备的设备号
 * @param[in]	block	逻辑块号
 * @retval		成功返回1，失败返回0
 */
static int free_dind(int dev, int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;
	int block_busy;

	if (!block) {
		return 1;
	}
	block_busy = 0;
	if ((bh = bread(dev, block))) {
		p = (unsigned short *) bh->b_data;	/* 指向缓冲块数据区 */
		for (i = 0; i < 512; i++, p++) {	/* 每个逻辑块上可连接512个二级块 */
			if (*p) {
				if (free_ind(dev, *p)) {	/* 释放所有一次间接块 */
					*p = 0;					/* 清零 */
					bh->b_dirt = 1;			/* 设置已修改标志 */
				} else {
					block_busy = 1;			/* 设置逻辑块没有释放标志 */
				}
			}
		}
		brelse(bh);							/* 释放二次间接块占用的缓冲块 */
	}
	/* 最后释放设备上的二次间接块。但如果其中有逻辑块没有被释放，则返回0(失败) */
	if (block_busy) {
		return 0;
	} else {
		return free_block(dev, block);		/* 最后释放存放第一间接块的逻辑块 */
	}
}

/** 
 * 截断文件数据函数
 * 将节点对应的文件长度减0，并释放占用的设备空间
 * @param[in]	inode
 * @retval  	void
 */
void truncate(struct m_inode * inode)
{
	int i;
	int block_busy;		/* 有逻辑块没有被释放的标志 */

	/* 如果不是常规文件、目录文件或链接项，则返回 */
	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
	     S_ISLNK(inode->i_mode))) {
		return;
	}
	
repeat:
	block_busy = 0;
	/* 释放i节点的7个直接逻辑块 */
	for (i = 0; i < 7; i++) {
		if (inode->i_zone[i]) {			/* 如果块号不为0，则释放之 */
			if (free_block(inode->i_dev, inode->i_zone[i])) {
				inode->i_zone[i] = 0;	/* 块指针置0 */
			} else {
				block_busy = 1;			/* 若没有释放掉则置标志 */
			}
		}
	}
	/* 释放所有一次间接块 */
	if (free_ind(inode->i_dev, inode->i_zone[7])) {
		inode->i_zone[7] = 0;			/* 块指针置0 */
	} else {
		block_busy = 1;					/* 若没有释放掉则置标志 */
	}
	/* 释放所有二次间接块 */
	if (free_dind(inode->i_dev, inode->i_zone[8])) {
		inode->i_zone[8] = 0;			/* 块指针置0 */
	} else {
		block_busy = 1;					/* 若没有释放掉则置标志 */
	}
	/* 设置i节点已修改标志，并且如果还有逻辑块由于“忙”而没有被释放，则把当前进程运行时间
	 片置0，以让当前进程先被切换去运行其他进程，稍等一会再重新执行释放操作 */
	inode->i_dirt = 1;
	if (block_busy) {
		current->counter = 0;			/* 当前进程时间片置0 */
		schedule();
		goto repeat;
	}
	inode->i_size = 0;					/* 文件大小置零 */
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

