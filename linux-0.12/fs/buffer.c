/*
 *  linux/fs/buffer.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'buffer.c' implements the buffer-cache functions. Race-conditions have
 * been avoided by NEVER letting a interrupt change a buffer (except for the
 * data, of course), but instead letting the caller do it. NOTE! As interrupts
 * can wake up a caller, some cli-sti sequences are needed to check for
 * sleep-on-calls. These should be extremely quick, though (I hope).
 */
/*
 * 'buffer.c'用于实现缓冲区高速缓存功能。通过不让中断处理过程改变缓冲区，而是让调用者来执行，避
 * 免了竞争条件(当然除改变数据外)。注意！由于中断可以唤醒一个调用者，因此就需要开关中断指令
 * (cli-sti)序列来检测由于调用而睡眠。但需要非常快(我希望是这样)。
 */


/*
 * NOTE! There is one discordant note here: checking floppies for
 * disk change. This is where it fits best, I think, as it should
 * invalidate changed floppy-disk-caches.
 */
/*
 * 注意！有一个程序应不属于这里：检测软盘是否更换。但我想这里是放置该程序最好的地方了，因为它需
 * 要使已更换软盘缓冲失效。
 */

#include <stdarg.h>

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/system.h>
#include <asm/io.h>

// buffer_wait变量是等待空闲缓冲块而睡眠的任务队列头指针。它与缓冲块头部结构中b_wait指针的作用
// 不同。当任务申请一个缓冲块而正好遇到系统缺乏可用空闲缓冲块时，当前任务就会被添加到buffer_wait
// 睡眠等待队列中。而b_wait则是专门供等待指定缓冲块(即b_wait对应的缓冲块)的任务使用的等待队列头
// 指针。
//
// NR_BUFFERS其值即是变量名nr_buffers，并且在include/linux/fs.h文件声明为全局变量。利用大写名
// 称来隐含地表示nr_buffers是一个在初始化之后不再改变的"常量"。它将在buffer_init()中被设置。

/* end是由编译时的连接程序ld生成，表明内核代码的末端 */
extern int end;	

/* 高速缓冲区开始于内核代码末端位置 */
struct buffer_head *start_buffer = (struct buffer_head *) &end;

/* 缓冲区Hash表数组 */
struct buffer_head *hash_table[NR_HASH];

/* 空闲缓冲块链表头指针 */
static struct buffer_head *free_list;

/* 等待空闲缓冲块而睡眠的任务队列 */
static struct task_struct *buffer_wait = NULL;

/* 系统所含缓冲个数 */
int NR_BUFFERS = 0;


// wait_on_buffer中，虽然是在关闭中断(cli)之后去睡眠的，但这样做并不会影响在其他进程上下文中响应
// 中断。因为每个进程都在自己的TSS段中保存了标志寄存器EFLAGS的值，所在在进程切换时CPU中当前
// EFLAGS的值也随之改变。
//
// 使用sleep_on()进入睡眠状态的进程需要用wake_up()明确地唤醒。

/**
 * 等待指定缓冲块解锁
 * 如果指定的缓冲块bh已经上锁就让进程不可中断地睡眠在该缓冲块的等待队列b_wait中。在缓冲块解锁时，
 * 其等待队列上的所有进程将被唤醒。
 * @param[in]	bh		指定缓冲块头指针
 * @retval		void
 */ 
static inline void wait_on_buffer(struct buffer_head * bh)
{
	cli();
	while (bh->b_lock) {	/* 如果已被上锁则进程进入睡眠，等待其解锁 */
		sleep_on(&bh->b_wait);
	}
	sti();
}

/**
 * 设备数据同步
 * 将内存高速缓冲区中的数据同步到设备中
 * @param[in]	void
 * @retval  	0
 */
int sys_sync(void)
{
	int i;
	struct buffer_head *bh;

	sync_inodes();					/* write out inodes into buffers */
									/* 将修改过的i节点写入缓冲区 */
	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++, bh++) {
		wait_on_buffer(bh);			/* 等待缓冲区解锁 */
		if (bh->b_dirt) {
			ll_rw_block(WRITE, bh); /* 产生写设备块请求 */
		}
	}
	return 0;
}

/**
 * 对指定设备进行数据同步
 * 该函数首先搜索高速缓冲区中所有缓冲块。对于指定设备dev的缓冲块，若其数据已被修改过就写入盘中
 * (同步操作)。然后把内存中i节点数据写入高速缓冲中。之后再指定设备dev执行一次与上述相同的写盘操
 * 作。
 * @param[in]	dev		设备号
 * @retval		0
 */
int sync_dev(int dev)
{
	int i;
	struct buffer_head *bh;

	/* 这里采用两遍同步操作是为了提高内核执行效率。第一遍缓冲区同步操作可以让内核中许多“脏块”
	 变干净，使得inode的同步操作能够高效执行。*/
	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++, bh++) {
		if (bh->b_dev != dev) {
			continue;
		}
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_dirt) {
			ll_rw_block(WRITE, bh);
		}
	}
	sync_inodes();
	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++, bh++) {
		if (bh->b_dev != dev) {
			continue;
		}
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_dirt) {
			ll_rw_block(WRITE, bh);
		}
	}
	return 0;
}

/**
 * 使指定设备在高速缓冲区中的数据无效
 * 扫描高速缓冲中所有的缓冲块。对指定设备的缓冲块复位其有效(更新)标志和修改标志。
 * @param[in]	dev		设备号
 * @retval  	void
 */
void inline invalidate_buffers(int dev)
{
	int i;
	struct buffer_head *bh;

	bh = start_buffer;
	for (i = 0; i < NR_BUFFERS; i++, bh++) {
		if (bh->b_dev != dev){
			continue;
		}
		wait_on_buffer(bh);
		/* 由于进程执行过睡眠等待，所以需要再判断一下缓冲区是否是指定设备 */
		if (bh->b_dev == dev) {
			bh->b_uptodate = bh->b_dirt = 0;
		}
	}
}

/*
 * This routine checks whether a floppy has been changed, and
 * invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 *
 * NOTE! Although currently this is only for floppies, the idea is
 * that any additional removable block-device will use this routine,
 * and that mount/open needn't know that floppies/whatever are
 * special.
 */
/*
 * 该子程序检查一个软盘是否已被更换，如果已经更换就使高速缓冲中与该软驱对应的所有缓冲区无效。该
 * 子程序相对来说较慢，所以我们要尽量少使用它。所以仅在执行'mount'或'open'时才调用它。我想这是
 * 将程度与实用性相结合的最好方法。若在操作过程中更换软盘，就会导致数据的丢失。这是咎由自取。
 *
 * 注意！尽管目前该子程序仅用于软盘，以后任何可移动介质的块设备都有将使用该程序，mount/open操作
 * 不需要知道是软盘还是其他什么特殊介质。
 */

/**
 * 检查磁盘是否更换
 * 检查磁盘是否更换，如果已更换就使用对应调整缓冲区无效
 * @param[in]	dev		设备号
 * @retval  	void
 */
void check_disk_change(int dev)
{
	int i;
	/* 首先检测一下是否为软盘设备。因为当时仅支持软盘可移动介质 */
	if (MAJOR(dev) != 2) {
		return;
	}
	/* 测试软盘是否已更换 */
	if (!floppy_change(dev & 0x03)) {
		return;
	}
	/* 软盘已更换，释放该设备的超级块 */
	for (i = 0; i < NR_SUPER; i++) {
		if (super_block[i].s_dev == dev) {
			put_super(super_block[i].s_dev);
		}
	}
	/* 同时释放对应设备的i节点位图和逻辑位图所占的高速缓冲区 */
	invalidate_inodes(dev);
	invalidate_buffers(dev);
}

// hash队列是双向链表结构，空闲缓冲块队列是双向循环链表结构。
//
// hash表的主要作用是减少查找比较元素所花费的时间。通过在元素的存储位置与关键字之间建立一个对应
// 关系(hash函数)，我们就可以直接通过函数计算立刻查询到指定的元素。建立函数的方法有多种，这里，
// Linux0.12主要采用了关键字除余数法。因为我们寻找的缓冲块有两个条件，即设备号dev和缓冲块号
// block，因此设计的hash函数肯定需要包含这两个关键值。这里两个关键字的异或操作只是计算关键值的
// 一种方法。再对关键值进行MOD运算就可以保证函数计算得到的值都处于函数数组项范围内。
#define _hashfn(dev, block) (((unsigned)(dev ^ block)) % NR_HASH)
#define hash(dev, block) 	hash_table[_hashfn(dev, block)]

/**
 * 从hash队列和空闲缓冲队列中移走缓冲块。
 * @param[in]	bh		要移除的缓冲区头指针
 * @retval 		void
 */
static inline void remove_from_queues(struct buffer_head * bh)
{
	/* remove from hash-queue */
	/* 从hash队列中移除缓冲块 */
	if (bh->b_next) {
		bh->b_next->b_prev = bh->b_prev;
	}
	if (bh->b_prev) {
		bh->b_prev->b_next = bh->b_next;
	}
	/* 如果该缓冲区是该队列的第一个块，则让hash表的对应项指向本队列中的下一个缓冲区 */
	if (hash(bh->b_dev,bh->b_blocknr) == bh) {
		hash(bh->b_dev,bh->b_blocknr) = bh->b_next;
	}
	/* remove from free list */
	/* 从空闲缓冲块表中移除缓冲块 */
	if (!(bh->b_prev_free) || !(bh->b_next_free)) {
		panic("Free block list corrupted");
	}
	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;
	/* 如果空闲链表头指向本缓冲区，则让其指向下一缓冲区 */
	if (free_list == bh) {
		free_list = bh->b_next_free;
	}
}

/**
 * 将缓冲块插入空闲链表尾部，同时放入hash队列中
 * @param[in]	bh		要插入的缓冲区头指针
 * @retval 		void
 */
static inline void insert_into_queues(struct buffer_head * bh)
{
	/* put at end of free list */
	/* 放在空闲链表末尾处 */
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh;
	/* put the buffer in new hash-queue if it has a device */
	/* 如果该缓冲块对应一个设备,则将其插入新hash队列中 */
	bh->b_prev = NULL;
	bh->b_next = NULL;
	if (!bh->b_dev) {
		return;
	}
	bh->b_next = hash(bh->b_dev,bh->b_blocknr);
	hash(bh->b_dev,bh->b_blocknr) = bh;
	/* bug修复！第一次hash()会返回NULL，需要判断一下 */
	if (bh->b_next) {	
		bh->b_next->b_prev = bh;
	}
}

/**
 * 在hash表查找指定缓冲块
 * @param[in] 	dev		设备号
 * @param[in] 	block 	块号
 * @retval 		如果找到则返回缓冲区块的指针，否则返回NULL
 */
static struct buffer_head * find_buffer(int dev, int block)
{		
	struct buffer_head * tmp;

	for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next) {
		if (tmp->b_dev == dev && tmp->b_blocknr == block) {
			return tmp;
		}
	}
	return NULL;
}

/*
 * Why like this, I hear you say... The reason is race-conditions.
 * As we don't lock buffers (unless we are readint them, that is),
 * something might happen to it while we sleep (ie a read-error
 * will force it bad). This shouldn't really happen currently, but
 * the code is ready.
 */
/*
 * 代码为什么会是这样子的？我听见你问...原因是竞争条件。由于我们没有对缓冲块上锁(除非我们正在读
 * 取它们的数据)，那么当我们(进程)睡眠时缓冲块可能发生一些问题(例如一个读错误将导致该缓冲块出错)。
 * 目前这种情况实际上是不会发生的，但处理的代码已经准备好了。
 */

/**
 * 利用hash表在高速缓冲区中寻找指定的缓冲块。
 * @param[in] 	dev		设备号
 * @param[in] 	block 	块号
 * @retval 如果找到则返回上锁后的缓冲区块的指针，否则返回NULL
 */
struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (;;) {
		if (!(bh = find_buffer(dev, block))) {
			return NULL;
		}
		bh->b_count ++;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_blocknr == block) {
			return bh;
		}
		bh->b_count--;
		#if 0
		// Q: 上面为什么不是这样? 
		// A: bh->b_count先自增，会告诉系统，这个块还要用，别释放。
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_blocknr == block) {
			bh->b_count ++;
			return bh;
		}
		#endif
	}
}

/*
 * Ok, this is getblk, and it isn't very clear, again to hinder
 * race-conditions. Most of the code is seldom used, (ie repeating),
 * so it should be much more efficient than it looks.
 *
 * The algoritm is changed: hopefully better, and an elusive bug removed.
 */
/*
 * OK，下面是getblk函数，该函数的逻辑并不是很清晰，同样也是因为要考虑竞争条件问题。其中大部分代
 * 码很少用到(例如重复操作语句)，因此它应该比看上去的样子有效得多。
 *
 * 算法已经作了改变：希望能更好，而且一个难以琢磨的错误已经去除。
 */
#define BADNESS(bh) (((bh)->b_dirt << 1) + (bh)->b_lock)

/**
 * 取高速缓冲中指定的缓冲块
 * 检查指定(设备号和块号)的缓冲区是否已经在高速缓冲中。如果指定块已经在高速缓冲中，则返回对应缓
 * 冲区头指针退出；如果不在，就需要在高速中中设置一个对应设备号和块号的新项。返回相应缓冲区头指
 * 针
 * @note 		在这里，每次进程执行过wait_on_buffer()睡眠等待，唤醒后需要重新判断等待的缓冲
 *				块是否符合条件。
 * @param[in] 	dev		设备号
 * @param[in] 	block 	块号
 * @retval	 	对应缓冲区头指针
 */
struct buffer_head * getblk(int dev, int block)
{
	struct buffer_head *tmp, *bh;

repeat:
	if ((bh = get_hash_table(dev, block))) {
		return bh;
	}
	tmp = free_list;
	do {
		if (tmp->b_count) {
			continue;
		}
		if (!bh || BADNESS(tmp) < BADNESS(bh)) {
			bh = tmp;
			if (!BADNESS(tmp)) {
				break;
			}
		}
	/* and repeat until we find something good */
	/* 重复操作直到找到适合的缓冲块 */
	} while ((tmp = tmp->b_next_free) != free_list);
	if (!bh) {
		sleep_on(&buffer_wait);
		goto repeat;
	}
	wait_on_buffer(bh);
	if (bh->b_count) {
		goto repeat;
	}
	while (bh->b_dirt) {
		sync_dev(bh->b_dev);
		wait_on_buffer(bh);
		if (bh->b_count) {
			goto repeat;
		}
	}
	/* NOTE!! While we slept waiting for this block, somebody else might */
	/* already have added "this" block to the cache. check it */
	/* 注意！当进程为了等待该缓冲块而睡眠时，其他进程可能已经将该缓冲块加入进高速缓冲中，所以我
	 们也要对此进行检查。 */
	if (find_buffer(dev, block)) {
		goto repeat;
	}
	/* OK, FINALLY we know that this buffer is the only one of it's kind, */
	/* and that it's unused (b_count=0), unlocked (b_lock=0), and clean */
	/* OK，最终我们知道该缓冲块是指定参数的唯一一块，而且目前还没有被占用(b_count=0)，也未被上
	 锁(b_lock=0)，并且是干净的(未被修改的) */
	bh->b_count = 1;
	bh->b_dirt = 0;
	bh->b_uptodate = 0;
	/* 从hash队列和空闲块链表中移出该缓冲头，让该缓冲区用于指定块。然后根据此新设备号和块号重新
	 插入空闲链表和hash队列新位置处，并最终返回缓冲头指针。*/
	remove_from_queues(bh);
	bh->b_dev = dev;
	bh->b_blocknr = block;
	insert_into_queues(bh);
	return bh;
}

/**
 * 释放指定缓冲块
 * 等待该缓冲块解锁。然后引用计数递减1，并明确地唤醒等待空闲缓冲块的进程。
 * @param[in] 	buf 	指定缓冲块
 * @retval 		void
 */
void brelse(struct buffer_head * buf)
{
	if (!buf) {
		return;
	}
	wait_on_buffer(buf);
	if (!(buf->b_count--)) {
		panic("Trying to free free buffer");
	}
	wake_up(&buffer_wait);
}

/*
 * bread() reads a specified block and returns the buffer that contains
 * it. It returns NULL if the block was unreadable.
 */
 /*
 * 从设备上读取指定的数据块并返回含有数据的缓冲区。如果指定的块不存在则返回NULL。
 */

/**
 * 从设备上读取指定数据块到高速缓冲区
 * @param[in] 	dev		设备号
 * @param[in] 	block	块号
 * @retval 		缓冲块头指针，失败返回NULL
 */
struct buffer_head * bread(int dev, int block)
{
	struct buffer_head * bh;

	if (!(bh = getblk(dev, block))) {
		panic("bread: getblk returned NULL\n");
	}
	if (bh->b_uptodate) {
		return bh;
	}
	ll_rw_block(READ, bh);
	wait_on_buffer(bh);
	if (bh->b_uptodate) {
		return bh;
	}
	brelse(bh);
	return NULL;
}

/**
 * 复制内存块
 * 从from地址复制一块(1024B)数据到to地址
 */
#define COPYBLK(from, to) 								\
__asm__(												\
	"cld\n\t" 											\
	"rep\n\t" 											\
	"movsl\n\t" 										\
	:													\
	:"c" (BLOCK_SIZE/4),"S" (from),"D" (to) 			\
	)

/*
 * bread_page reads four buffers into memory at the desired address. It's
 * a function of its own, as there is some speed to be got by reading them
 * all at the same time, not waiting for one to be read, and then another
 * etc.
 */
/*
 * bread_page 一次读四个缓冲块数据读到内存指定的地址处。它是一个完整的函数，因为同时读取四块可以
 * 获得速度上的好处，不用等着读一块，再读一块了。
 */

/**
 * 读设备的一个页面的内容到指定内存地址处
 * @note 该函数仅用于mm/memory.c文件的do_no_page()函数中
 * @param[in] 	address	保存页面数据的地址
 * @param[in] 	dev		设备号
 * @param[in] 	b[4]	含有4个设备数据块号的数组
 * @retval 		void
 */
void bread_page(unsigned long address, int dev, int b[4])
{
	struct buffer_head * bh[4];
	int i;

	/* 从高速缓冲中取指定设备和块号的的缓冲块。如果缓冲块中数据无效(未更新)，则产生读设备请求从
	 设备上读取相应数据块 */
	for (i = 0; i < 4; i ++) {
		if (b[i]) {
			if ((bh[i] = getblk(dev, b[i]))) {
				if (!bh[i]->b_uptodate) {
					ll_rw_block(READ, bh[i]);
				}
			}
		} else {
			bh[i] = NULL;
		}
	}
	/* 随后将4个缓冲块上的内容顺序复制到指定地址处，随后释放相应缓冲块 */
	for (i = 0; i < 4; i++, address += BLOCK_SIZE) {
		if (bh[i]) {
			wait_on_buffer(bh[i]);
			if (bh[i]->b_uptodate) {
				COPYBLK((unsigned long) bh[i]->b_data, address);
			}
			brelse(bh[i]);
		}
	}
}

/*
 * Ok, breada can be used as bread, but additionally to mark other
 * blocks for reading as well. End the argument list with a negative
 * number.
 */
/**
 * 从指定设备读取指定的一些块
 * @param[in]	dev		设备号
 * @param[in]	first	要读取的第一个块号
 * @param[in]	...		要预读取的一系列块号
 * @retval 		成功返回第1块的缓冲块头指针，失败返回NULL。
 */
struct buffer_head * breada(int dev, int first, ...)
{
	va_list args;
	struct buffer_head * bh, *tmp;

	va_start(args, first);
	/* 读取第一块缓冲块 */
	if (!(bh = getblk(dev, first))) {
		panic("bread: getblk returned NULL\n");
	}
	if (!bh->b_uptodate) {
		ll_rw_block(READ, bh);
	}
	/* 预读取可变参数表中的其他预读块号，但不引用 */
	while ((first = va_arg(args, int)) >= 0) {
		tmp = getblk(dev, first);
		if (tmp) {
			if (!tmp->b_uptodate) {
				ll_rw_block(READA, tmp); /* bug修复! 这里的 bh 改为 tmp */
			}
			tmp->b_count --; /* 暂时释放掉该预读块 */
		}
	}
	va_end(args);
	wait_on_buffer(bh);
	/* 等待之后，缓冲区数据仍然有效，则返回 */
	if (bh->b_uptodate) {
		return bh;
	}
	brelse(bh);
	return (NULL);
}

/**
 * 缓冲区初始化
 * 缓冲区低端内存被初始化成缓冲头部，缓冲区高端内存被初始化缓冲区。
 * @note 		buffer_end是一个>=1M的值。该初始化函数在init/main.c调用。
 * @param[in]	buffer_end	高速缓冲区结束的内存地址
 */
void buffer_init(long buffer_end)
{
	struct buffer_head * h = start_buffer;
	void * b;
	int i;
	/* 跳过640KB~1MB的内存空间，该段空间被显示内存和BIOS占用 */
	if (buffer_end == 1<<20) {
		b = (void *) (640*1024);
	}
	else {
		b = (void *) buffer_end;
	}
	while ( (b -= BLOCK_SIZE) >= ((void *) (h+1)) ) {
		h->b_dev = 0;
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
		h->b_next = NULL;
		h->b_prev = NULL;
		h->b_data = (char *) b;
		/* 以下两句形成双向链表 */
		h->b_prev_free = h - 1;
		h->b_next_free = h + 1;
		h ++;
		NR_BUFFERS ++;
		/* 同样为了跳过 640KB~1MB 的内存空间 */
		if (b == (void *) 0x100000)
			b = (void *) 0xA0000;
	}
	h --;						/* 让h指向最后一个有效缓冲块头 */
	free_list = start_buffer;	/* 让空闲链表头指向头一个缓冲块 */
	free_list->b_prev_free = h; /* 链表头的b_prev_free指向前一项(即最后一项) */
	h->b_next_free = free_list; /* 表尾指向表头，形成环形双向链表 */
	/* 初始化hash表 */
	for (i = 0; i < NR_HASH; i++) {
		hash_table[i]=NULL;
	}
}	
