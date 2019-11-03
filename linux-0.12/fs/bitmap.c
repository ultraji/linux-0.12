/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
/* bitmap.c程序含有处理i节点和磁盘块位图的代码 */
#include <string.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/* 将指定地址(addr)处的一块1024字节内存清零 */
#define clear_block(addr) 												\
	__asm__(															\
		"cld\n\t"         												\
		"rep\n\t" 														\
		"stosl" 														\
		::"a" (0), "c" (BLOCK_SIZE / 4), "D" ((long) (addr)))

/**
 * 把指定地址开始的第nr个位偏移处的比特位置位(nr可大于32!)
 * btsl指令用于测试并设置位(Bit Test and Set)。把基地址(%3)和位偏移值(%2)所指定的位值先保存到进
 * 位标志CF中，然后设置该位为1。指令setb用于根据进位标志CF设置操作数(%al)。如果CF=1，则%al=1，
 * 否则%al=0。
 * @param[in]	nr		位偏移
 * @param[in]	addr	指定地址的基地址
 * @retval		返回addr+nr处比特位的原位值	
 */
#define set_bit(nr, addr) ({											\
	register int res; 													\
	__asm__ __volatile__("btsl %2, %3\n\tsetb %%al"						\
				:"=a" (res)												\
				:"0" (0),"r" (nr),"m" (*(addr))); 						\
		res;})

/**
 * 复位指定地址开始的第nr位偏移处的位，返回原位值的反码
 * @param[in]	nr		位偏移	
 * @param[in]	addr	指定地址的基地址
 * @retval		返回addr+nr处比特位的原位值的反码
 */
#define clear_bit(nr, addr) ({											\
	register int res;													\
	__asm__ __volatile__("btrl %2, %3\n\tsetnb %%al"					\
			:"=a" (res) 												\
			:"0" (0), "r" (nr), "m" (*(addr))); 						\
		res;})

/**
 * 从addr开始寻找第1个0值位
 * 在addr指定地址开始的位图中寻找第1个是0的位，并将其距离addr的位偏移值返回。addr是缓冲块数据区
 * 的地址，扫描寻找的范围是1024字节(8192位)。
 * @param[in]	addr	指定地址
 * @retval		返回第一个0值位距离addr的位偏移值
 */
#define find_first_zero(addr) ({ 										\
	int __res; 															\
	__asm__(															\
			"cld\n"														\
		"1:\tlodsl\n\t"													\
			"notl %%eax\n\t"                							\
			"bsfl %%eax, %%edx\n\t"										\
			"je 2f\n\t"													\
			"addl %%edx, %%ecx\n\t"										\
			"jmp 3f\n"													\
		"2:\taddl $32, %%ecx\n\t"										\
			"cmpl $8192, %%ecx\n\t"										\
			"jl 1b\n"													\
		"3:"															\
		:"=c" (__res)													\
		:"c" (0), "S" (addr)); 											\
	__res;})

/**
 * 释放设备dev上数据区中的逻辑块block
 * @param[in]	dev		设备号
 * @param[in]	block	逻辑块号(盘块号)
 * @retval		成功返回1，失败返回0
 */
int free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!(sb = get_super(dev))) {
		panic("trying to free block on nonexistent device");
	}
	if (block < sb->s_firstdatazone || block >= sb->s_nzones) {
		panic("trying to free block not in datazone");
	}
	/* 从hash表中寻找该块数据 */
	bh = get_hash_table(dev, block);
	if (bh) {
		if (bh->b_count > 1) {	/* 引用次数大于1，该块还有人用，则调用brelse()后退出 */
			brelse(bh);
			return 0;
		}
		bh->b_dirt = 0;
		bh->b_uptodate = 0;
		if (bh->b_count) {		/* 若此时b_count为1, 则调用brelse()释放之 */
			brelse(bh);
		}
	}
	/* 接着复位block在逻辑块位图中的位(置0) */
	block -= sb->s_firstdatazone - 1 ;
	if (clear_bit(block & 8191, sb->s_zmap[block/8192]->b_data)) {
		printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
		printk("free_block: bit already cleared\n");
	}
	/* 最后置相应逻辑块位图所在缓冲区的已修改标志 */
	sb->s_zmap[block/8192]->b_dirt = 1;
	return 1;
}

/**
 * 向设备dev申请一个逻辑块
 * @param[in]	dev		设备号
 * @retval		成功返回逻辑块号，失败返回0。
 */
int new_block(int dev)
{
	struct buffer_head * bh;
	struct super_block * sb;
	int i, j;

	if (!(sb = get_super(dev))) {
		panic("trying to get new block from nonexistant device");
	}
	/* 扫描文件系统的8块逻辑块位图，寻找首个0值位，以寻找空闲逻辑块，获取设置该逻辑块的块号 */
	j = 8192;
	for (i = 0 ; i < 8 ; i++) {
		if ((bh = sb->s_zmap[i])) {
			if ((j = find_first_zero(bh->b_data)) < 8192) {
				break;
			}
		}
	}
	/* 然后如果全部扫描完8块逻辑块位图的所有位还没有找到0值位或者位图所在的缓冲块指针无效
	 (bn = NULL)则表示当前没有空闲逻辑块 */
	if (i >= 8 || !bh || j >= 8192) {
		return 0;
	}
	/* 设置找到的新逻辑块j对应逻辑块位图中的位，若对应位已经置位，则出错停机 */
	if (set_bit(j, bh->b_data)) {
		panic("new_block: bit already set");
	}
	bh->b_dirt = 1;
	/* 计算该块在逻辑块位图中位偏移值，偏移值大于该设备上的总逻辑块数，则出错 */
	j += i * 8192 + sb->s_firstdatazone - 1;
	if (j >= sb->s_nzones) {
		return 0;
	}
	/* 在高速缓冲区中为该设备上指定的逻辑块号取得一个缓冲块，并返回缓冲块头指针 */
	if (!(bh = getblk(dev, j))) {
		panic("new_block: cannot get block");
	}
	/* 因为新取出的逻辑块其引用次数一定为1，若不是1，说明内核有问题。*/
	if (bh->b_count != 1) {
		panic("new block: count is != 1");
	}
	/* 将新逻辑块清零，并设置其已更新标志和已修改标志。然后释放对应缓冲块，返回逻辑块号 */
	clear_block(bh->b_data);
	bh->b_uptodate = 1;
	bh->b_dirt = 1;
	brelse(bh);
	return j;
}

// 下面两个函数与上面逻辑块操作类似，只是对象换成了i节点

/**
 * 释放指定的i节点
 * @param[in] 	inode 	指向要释放的i节点的指针
 * @retval 		void
 */
void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!inode) {
		return;
	}
	/* i节点上的设备号字段为0,说明该节点没有使用 */
	if (!inode->i_dev) {
		memset(inode, 0, sizeof(*inode));
		return;
	}
	/* 如果此i节点还有其他程序引用，则不释放，说明内核有问题，停机 */ 
	if (inode->i_count > 1) {
		printk("trying to free inode with count=%d\n", inode->i_count);
		panic("free_inode");
	}
	/* 如果文件连接数不为0,则表示还有其他文件目录项在使用该节点，因此也不应释放，而应该放回等 */
	if (inode->i_nlinks) {
		panic("trying to free inode with links");
	}
	if (!(sb = get_super(inode->i_dev))) {
		panic("trying to free inode on nonexistent device");
	}
	if (inode->i_num < 1 || inode->i_num > sb->s_ninodes) {
		panic("trying to free inode 0 or nonexistant inode");
	}
	/* 找到inode所在的逻辑块，其中i_num>>13即i_num/8192 */
	if (!(bh = sb->s_imap[inode->i_num >> 13])) { 
		panic("nonexistent imap in superblock");
	}
	/* 现在我们复位i节点对应的节点位图中的位 */
	if (clear_bit(inode->i_num & 8191, bh->b_data)) {
		printk("free_inode: bit already cleared.\n\r");
	}
	/* 置i节点位图所在缓冲区已修改标志，并清空该i节点结构所占内存区 */
	bh->b_dirt = 1;
	memset(inode, 0, sizeof(*inode));
}

/**
 * 在设备dev上创建一个新i节点
 * @param[in]	dev		设备号
 * @retval		成功返回新i节点的指针，失败返回NULL
 */
struct m_inode * new_inode(int dev)
{
	struct m_inode * inode;
	struct super_block * sb;
	struct buffer_head * bh;
	int i, j;

	/* 首先从内存i节点表(inode_table)中获取一个空闲i节点项，并读取指定设备的超级块结构。*/ 
	if (!(inode = get_empty_inode())) {
		return NULL;
	}
	if (!(sb = get_super(dev))) {
		panic("new_inode with unknown device");
	}
	/*扫描超级块中8块i节点位图，寻找第1个0位(空闲节点)，获取并设置该i节点的节点号。*/
	j = 8192;
	for (i = 0 ; i < 8 ; i++) {
		if ((bh = sb->s_imap[i])) {
			if ((j = find_first_zero(bh->b_data)) < 8192) {
				break;
			}
		}
	}
	/* 如果全部扫描完还没找到空闲i节点或者位图所在的缓冲块无效(bh = NULL)，则放回先前申请的i节
	 点表中的i节点，并返回空指针退出 */
	if (!bh || j >= 8192 || j + i * 8192 > sb->s_ninodes) {
		iput(inode);
		return NULL;
	}
	/* 现在已经找到了还未使用的i节点号j。于是置位i节点j对应的i节点位图相应比特位。然后置i节点位
	 图所在缓冲块已修改标志 */
	if (set_bit(j, bh->b_data)) {
		panic("new_inode: bit already set");
	}
	bh->b_dirt = 1;
	/* 初始化该i节点结构 */
	inode->i_count = 1;
	inode->i_nlinks = 1;
	inode->i_dev = dev;
	inode->i_uid = current->euid;
	inode->i_gid = current->egid;
	inode->i_dirt = 1;
	inode->i_num = j + i * 8192;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	return inode;
}
