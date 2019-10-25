/*
 *  linux/kernel/blk_dev/ll_rw.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This handles all read/write requests to block devices
 */
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

/*
 * The request-struct contains all necessary data
 * to load a nr of sectors into memory
 */
/*
 * 请求结构中含有加载 nr 个扇区数据到内存中去的所有必须的信息。
 */
/* 请求项数组队列，共有NR_REQUEST = 32个请求项 */
struct request request[NR_REQUEST];

/*
 * used to wait on when there are no free requests
 */
/*
 * 是用于在请求数组没有空闲项时进程的临时等待处。
 */
struct task_struct * wait_for_request = NULL;

/* blk_dev_struct is:
 *	do_request-address
 *	next-request
 */
/*
 * blk_dev_struct块设备结构是:(参见文件kernel/blk_drv/blk.h)
 * do_request-address	// 对应主设备号的请求处理程序指针
 * current-request		// 该设备的下一个请求
 */
// 块设备数组。该数组使用主设备号作为索引。实际内容将在各块设备驱动程序初始化时填入。
// 例如，硬盘驱动程序初始化时(hd.c)，第一条语句即用于设备blk_dev[3]的内容。
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		/* no_dev */		/* 0 - 无设备 */
	{ NULL, NULL },		/* dev mem */		/* 1 - 内存 */
	{ NULL, NULL },		/* dev fd */		/* 2 - 软驱设备 */
	{ NULL, NULL },		/* dev hd */		/* 3 - 硬盘设备 */
	{ NULL, NULL },		/* dev ttyx */		/* 4 - ttyx设备 */
	{ NULL, NULL },		/* dev tty */		/* 5 - tty设备 */
	{ NULL, NULL }		/* dev lp */		/* 6 - lp打印机设备 */
};

/*
 * blk_size contains the size of all block-devices:
 *
 * blk_size[MAJOR][MINOR]
 *
 * if (!blk_size[MAJOR]) then no minor size checking is done.
 */
/*
 * blk_size数组含有所有块设备的大小(块总数):
 * blk_size[MAJOR][MINOR]
 * 如果(!blk_size[MAJOR]),则不必检测子设备的块总数.
 */
// 设备数据块总数指针数组.每个指针项指向指定主设备号的总块数组.该总块数数组每一项对应子
// 设备号确定的一个子设备上所拥有的数据总数(1块大小 = 1KB).
int * blk_size[NR_BLK_DEV] = { NULL, NULL, };

// 锁定指定缓冲块
//
// 如果指定的缓冲块已经被其他任务锁定,则使自己睡眠(不可中断的等待),直到被执行解锁
// 缓冲块的任务明确地唤醒

static inline void lock_buffer(struct buffer_head * bh)
{
	cli();							/* 清中断许可 */
	while (bh->b_lock){				/* 如果缓冲区已被锁定则睡眠，直到缓冲区解锁 */
		sleep_on(&bh->b_wait);
	}
	bh->b_lock = 1;					/* 立刻锁定缓冲区 */
	sti();							/* 开中断 */
}

// 释放(解锁)锁定的缓冲区
//
// 该函数与hlk.h文件中的同名函数完全一样。
static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)				// 如果该缓冲区没有被锁定,则打印出错信息.
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;					// 清锁定标志.
	wake_up(&bh->b_wait);			// 唤醒等待该缓冲区的任务.
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 *
 * Note that swapping requests always go before other requests,
 * and are done in the order they appear.
 */
/*
 * add-request()向链表中加入一项请求项.它会关闭中断,这样就能安全地处理请求链表了.
 *
 * 注意,交换请求总是在其他请求之前操作,并且以它们出现在顺序完成.
 */
// 向链表中加入请求项.
// 参数dev是指定块设备结构指针,该结构中有处理请求项函数指针和当前正在请求项指针;
// req是已设置好内容的请求项结构指针.
// 本函数把已经设置好的请求项req添加到指定设备的请求项链表中.如果该设备在当前请求项指针为空,则可以设置req为当前请求项并立刻调用设备请求
// 项处理函数.否则就把req请求项插入到该请求项链表中.
static void add_request(struct blk_dev_struct * dev, struct request * req)
{
	// 首先对参数提供的请求项的指针和标志作初始设置.置空请求项中的下一请求项指针,关中断并清除请求项相关缓冲区脏标志.
	struct request * tmp;

	req->next = NULL;
	cli();								// 关中断
	if (req->bh)
		req->bh->b_dirt = 0;			// 清缓冲区"脏"标志.
	// 然后查看指定设备是否有当前请求项,即查看设备是否正忙.如果指定设备dev当前请求项(current_equest)字段为空,则表示目前该设备没有请求项,本次是
	// 第1个请求项,也是唯一的一个.因此可将块设备当前请求指针直接指向该请求项,并立刻执行相应设备的请求函数.
	if (!(tmp = dev->current_request)) {
		dev->current_request = req;
		sti();							// 开中断.
		(dev->request_fn)();			// 执行请求函数,对于硬盘是do_hd_request().
		return;
	}
	// 如果目前该设备已经有当前请求项在处理,则首先利用电梯算法搜索最佳插入位置,然后将当前请求项插入到请求链表中.在搜索过程中,如果判断出欲插入
	// 请求项的缓冲块头指针空,即没有缓冲块,那么就需要找一个项,其已经有可用的缓冲块.因此若当前插入位置(tmp之后)处的空闲项缓冲块头指针不空,就选择这个位置
	// 于是退出循环并把请求项插入此处.最后开中断并退出函数.电梯算法的作用是让磁盘磁头的移动距离最小,从而改善(减少)硬盘访问时间.
	// 下面for循环中if语句用于把req所指请求项与请求队列(链表)中已有的请求项作比较,找出req插入该队列的正确位置顺序.然后中断循环,并把req插入到该队列正确位置处.
	for ( ; tmp->next ; tmp = tmp->next) {
		if (!req->bh) {
			if (tmp->next->bh) {
				break;
			} else {
				continue;
			}
		}
		if ((IN_ORDER(tmp, req)||!IN_ORDER(tmp, tmp->next)) && IN_ORDER(req, tmp->next))
			break;
	}
	req->next = tmp->next;
	tmp->next = req;
	sti();
}

// 创建请求项并插入请求队列中.
// 参数major是主设备号;rw是指定命令;bh是存放数据的缓冲区头指针.
static void make_request(int major, int rw, struct buffer_head * bh)
{
	struct request * req;
	int rw_ahead;

	/* WRITEA/READA is special case - it is not really needed, so if the */
	/* buffer is locked, we just forget about it, else it's a normal read */
	/* WRITEA/READA是一种特殊情况 - 它们并非必要,所以如果缓冲区已经上锁,我们就不用管它,否则它只是一个一般的读操作. */
	// 这里'READ'和'WRITE'后面的'A'字符代表英文单词Ahead,表示预读/写数据块的意思.
	// 该函数首先对命令READA/WRITEA的情况进行一些处理.对于这两个命令,当指定的缓冲区正在使用而已被上锁时,就放弃预读/写请求.否则就作为普通
	// READ/WRITE命令进行操作.另外,如果参数给出的命令既不是READ也不是WRITE,则表示内核程序有错,显示出错信息并停机.注意,在修改命令之前这里
	// 已为参数是否为预读/写命令设置了标志rw_ahead.
	if ((rw_ahead = (rw == READA || rw == WRITEA))) {
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
	if (rw != READ && rw != WRITE)
		panic("Bad block dev command, must be R/W/RA/WA");
	lock_buffer(bh);                				// 锁定缓冲块
	// 如果是WRITE操作并且缓冲块未修改，或是READ操作并且缓冲块已更新，则直接返回缓冲区块。
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		unlock_buffer(bh);
		return;
	}
repeat:
	/* we don't allow the write-requests to fill up the queue completely:
	 * we want some room for reads: they take precedence. The last third
	 * of the requests are only for reads.
	 */
	/*
	 * 我们不能让队列中全都是写请求项:我们需要为读请求保留一些空间:读操作是优先的.请求队列的后三分之一空间仅用于读请求项.
	 */
	// 好,现在我们必须为本函数生成并添加读/写请求项了.首先我们需要在请求数组中寻找到一个空闲项(糟)来存放新请求项.搜索过程从请求数组末端开始.
	// 根据上述要求,对于读命令请求,我们直接从队列末尾开始搜索,而对于写请求就只能从队列2/3处向队列头处搜索空项填入.于是我们开始从后向前搜索,
	// 当请求结构request的设备字段dev值=-1时,表示该项未被占用(空闲).如果没有一项是空闲的(此时请求项数组指针已经搜索越过头部),则查看此次请求
	// 是否是提前读/写(READA或WRITEA),如果是则放弃此次请求操作.否则让本次请求操作先睡眠(以等待请求队列腾出空项),过一会儿再来搜索请求队列.
	if (rw == READ)
		req = request + NR_REQUEST;						// 对于读请求,将指针指向队列尾部.
	else
		req = request + ((NR_REQUEST * 2) / 3);			// 对于写请求,指针指向队列2/3处.
	/* find an empty request */
	/* 搜索一个空请求项 */
	while (--req >= request)
		if (req->dev < 0)
			break;
	/* if none found, sleep on new requests: check for rw_ahead */
	/* 如果没有找到空闲项,则让该次请求操作睡眠:需检查是否提前读/写 */
	if (req < request) {								// 如果已搜索到头(队列无空项)
		if (rw_ahead) {									// 则若是提前读/写请求,则退出.
			unlock_buffer(bh);
			return;
		}
		sleep_on(&wait_for_request);					// 否则就睡眠,过会再查看请求队列.
		goto repeat;
	}
	/* fill up the request-info, and add it to the queue */
	/* 向空闲请求项中填写请求信息,并将其加入队列中 */
	// OK,程序执行到这里表示已找到一个空闲请求项.于是我们在设置好的新请求项后就调用add_request()把它添加到请求队列中,立刻退出.请求结构请参见blk_drv/blk.h.
	// req->sector是读写操作的起始扇区号,req->buffer是请求项存放数据的缓冲区.
	req->dev = bh->b_dev;								// 设备号.
	req->cmd = rw;										// 命令(READ/WRITE).
	req->errors = 0;									// 操作时产生的错误次数.
	req->sector = bh->b_blocknr << 1;					// 起始扇区.块号转换成扇区号(1块=2扇区).
	req->nr_sectors = 2;								// 本请求项需要读写的扇区数.
	req->buffer = bh->b_data;							// 请求项缓冲区指针指向需读写的数据缓冲区.
	req->waiting = NULL;								// 任务等待操作执行完成的地方.
	req->bh = bh;										// 缓冲块头指针.
	req->next = NULL;									// 指向下一请求项.
	add_request(major + blk_dev, req);					// 将请求项加入队列中(blk_dev[major],reg).
}

// 低级页面读写函数(Low Level Read Write Pagk).
// 以页面(4K)为单位访问设备数据,即每次读/写8个扇区.参见下面ll_rw_blk()函数.
void ll_rw_page(int rw, int dev, int page, char * buffer)
{
	struct request * req;
	unsigned int major = MAJOR(dev);

	// 首先对函数参数的合法性进行检测.如果设备主设备号不存在或者该设备的请求操作函数不存在,则显示出错信息,并返回.如果参数给出的命令既不是
	// READ也不是WRITE,则表示内核程序有错,显示出错信息并停机.
	if (major >= NR_BLK_DEV || !(blk_dev[major].request_fn)) {
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}
	if (rw != READ && rw != WRITE)
		panic("Bad block dev command, must be R/W");
	// 在参数检测操作完成后,我们现在需要为本次操作建立请求项.首先我们需要在请求数组中寻找到一个空闲项(糟)来存放新请求项.搜索过程从请求数组末端
	// 开始.于是我们开始从后向前搜索,当请求结构request的设备字段值<0时,表示该项未被占用(空闲).如果没有一项是空闲的(此时请求项数组指针已经搜索越过
	// 头部),则让本次请求操作先睡眠(以等待请求队列腾出空项),过一会再来搜索请求队列.
repeat:
	req = request + NR_REQUEST;							// 将指针指向队列尾部.
	while (--req >= request)
		if (req->dev < 0)
			break;
	if (req < request) {
		sleep_on(&wait_for_request);					// 睡眠,过会再查看请求队列.
		goto repeat;
	}
	/* fill up the request-info, and add it to the queue */
	/* 向空闲请求项中填写请求信息,并将其加入队列中 */
	// OK,程序执行到这里表示已找到一个空闲请求项.于是我们设置好新请求项,把当前进程置为不可中断睡眠中断后,就去调用add_request()把它添加到请求队列中,
	// 然后直接调用调度函数让当前进程睡眠等待页面从交换设备中读入.这里不像make_request()函数那样直接退出函数而调用了schedule(),是因为make_request()
	// 函数仅读2个扇区数据.而这里需要对交换设备读/写8个扇区,需要花较长的时间.因此当前进程肯定需要等待而睡眠.因此这里直接就让进程去睡眠了,省得在程序其他地方
	// 还要进行这些判断操作.
	req->dev = dev;										// 设备号
	req->cmd = rw;										// 命令(READ/WRITE)start_code
	req->errors = 0;									// 读写操作错误计数
	req->sector = page << 3;							// 起始读写扇区
	req->nr_sectors = 8;								// 读写扇区数
	req->buffer = buffer;								// 数据缓冲区
	req->waiting = current;								// 当前进程进入该请求等待队列
	req->bh = NULL;										// 无缓冲块头指针(不用高速缓冲)
	req->next = NULL;									// 下一个请求项指针
	current->state = TASK_UNINTERRUPTIBLE;				// 置为不可中断状态
	add_request(major + blk_dev, req);					// 将请求项加入队列中.
	// 当前进程需要读取8个扇区的数据因此需要睡眠，因此调用调度程序选择进程运行
	schedule();
}

// 低级数据块读写函数(Low Level Read Write Block)
// 该函数是块设备驱动程序与系统其他部分的接口函数.通常在fs/buffer.c程序中被调用.
// 主要功能是创建块设备读写请求项并插入到指定块设备请求队列.实际的读写操作则是由设备的request_fn()函数完成.对于硬盘操作,该函数是do_hd_request();对于软盘操作
// 该函数是do_fd_request();对于虚拟盘则是do_rd_request().另外,在调用该函数之前,调用者需要首先把读/写块设备的信息保存在缓冲块头结构中,如设备号,块号.
// 参数:rw - READ,READA,WRITE或WRITEA是命令;bh - 数据缓冲块头指针.
void ll_rw_block(int rw, struct buffer_head * bh)
{
	unsigned int major;	/* 主设备号(对于硬盘是3) */

	// 如果设备主设备号不存在或者该设备的请求操作函数不存在,则显示出错信息,并返回.否则创建请求项并插入请求队列 */
	if ((major = MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
	!(blk_dev[major].request_fn)) {
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}
	make_request(major, rw, bh);
}

/* 初始化块设备的请求数组 */
void blk_dev_init(void)
{
	int i;

	for (i = 0; i < NR_REQUEST; i++) {
		request[i].dev = -1;
		request[i].next = NULL;
	}
}
