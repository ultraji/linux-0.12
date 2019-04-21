/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */
/*
 * 需求加载是从91.12.1开始编写 - 在程序编制表中似乎是最重要的程序，并且应该是很容易编制的 - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */
/*
 * Ok，需求加载是比较容易编写的，而共享页面却需要有点技巧。共享页面程序是91.12.2开始编写的，好
 * 像能够工作 - Linus。
 *
 * 通过执行大约30个/bin/sh对共享操作进行了测试：在老内核当中需要占用多于6MB的内在，而目前却不
 * 用。现在看来工作得很好。
 *
 * 对“invalidate()”函数也进行了修改--在这方面我还做得不够。
 */

/*
 * Real VM (paging to/from disk) started 18.12.91. Much more work and
 * thought has to go into this. Oh, well..
 * 19.12.91  -  works, somewhat. Sometimes I get faults, don't know why.
 *		Found it. Everything seems to work now.
 * 20.12.91  -  Ok, making the swap-device changeable like the root.
 */
/*
 * 91.12.18开始编写真正的虚拟内存管理VB（交换页面到/从磁盘）。需要对此考虑很多并且需要作很多
 *				工作。呵呵，也只能这样了。
 * 91.12.19 - 在某种程序上可以工作了，但有时会出错，不知道怎么回事。找到错误了，现在好像一切
 				都能工作了。
 * 91.12.20 - OK，把交换设备修改成可更改的了，就像根文件设备那样。
 */

#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

/* 该宏用于判断给定线性地址是否位于当前进程的代码段中，“(((addr)+4095)&~4095)”用于取得线性
 地址addr所在内存页面的末端地址 */
#define CODE_SPACE(addr) \
	((((addr) + 4095) & ~4095) < current->start_code + current->end_code)

unsigned long HIGH_MEMORY = 0;/* 全局变量，存放实际物理内存最高端地址 */

/* 从 from 处复制一页内存到 to 处(4KB) */
#define copy_page(from, to) \
		__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))


/* 内存映射字节图(1字节代表1页内存) */
unsigned char mem_map [ PAGING_PAGES ] = {0, };

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
/*
 * 释放物理地址"addr"处的一页内存。用于函数 free_page_tables()。
 */
/**
 * 释放物理地址addr开始的1页面内存
 * 物理地址1MB以下的内存空间用于内核程序和缓冲，不作为分配页面的内存空间。因此参数addr需要大于1MB。
 * @param[in]	addr	需要释放的起始物理地址
 * @retval		void
 */
void free_page(unsigned long addr)
{
	// 首先判断参数给定的物理地址 addr 的合理性。如果物理地址 addr 小于内存低端(1MB)，则表示在内
	// 核程序或高速缓冲中，对此不予处理。如果物理地址 addr >=系统所含物理内存最高端，则显示出错信
	// 息并且内核停止工作。
	if (addr < LOW_MEM) {
		return;
	}
	if (addr >= HIGH_MEMORY) {
		panic("trying to free nonexistent page");
	}
	// 如果对参数 addr 验证通过，那么就根据这个物理地址换算出内存低端开始计起的内存页面号。
	// 页面号 = (addr - LOW_MEME)/4096。可见页面号从0号开始计起.此时addr中存放着页面号。如果该
	// 页面号对应的页面映射字节不等于0，则减1返回。此时该映射字节值应该为0，表示页面已释放。如果
	// 对应页面原本就是0，表示该物理页面本来就是空闲的，说明内核代码出问题。于是显示出错信息并停机。
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--) {
		return;
	}
	// 执行到此处表示要释放空闲的页面，则将该页面的引用次数重置为 0
	mem_map[addr] = 0;
	panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
/*
 * 下面函数释放页表连续的内存块，exit() 需要该函数。与 copy_page_tables() 类似，该函数仅处理
 *  4MB 长度的内存块。
 */
// 根据指定的线性地址和限长(页表个数)，释放对应内存页表指定的内存块并置项空闲。
// 页目录位于物理地址0开始处，共1024项,每项4字节，共占4KB。每个目录项指定一个页表。内核页表物理地
// 址0x1000处开始(紧接着目录空间)，共4个页表。每个页表有1024项，每项4B。因此也占4KB(1页)内存。
// 各进程(除了在内核代码中的进程0和1)的页表所占据的页面在进程被创建时由内核为其在主内存区申请得到。
// 每个页表项对应1页物理内存，因此一个页表最多可映射4MB的物理内存。
// 参数: from - 起始线性基地址
// 		size - 释放的字节长度
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	// 首先检测参数from给出的线性基地址是否在4MB的边界处，因为该函数只能处理这种情况。若from = 0，则
	// 出错。说明试图释放内核和缓冲所占空间。
	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");
	// 然后计算参数size给出的长度所占的页目录数(4MB的进位整数倍)，也即所占页表数。
	// 因为1个页表可管理4MB物理内存，所以这里用右移22位的方式把需要复制的内存长度值除以4MB。其中加
	// 上0x3fffff(即4MB-1)用于得到进位整数倍结果，即除操作若有余数则进1。例如，如果原size = 4.01MB，
	// 那么可得到结果size = 2.
	size = (size + 0x3fffff) >> 22;
	// 接着计算给出的线性基地址对应的起始目录项，对应的目录项号 = from >>22。因为每项点4字节，并且
	// 由于页目录表从物理地址0开始存放，因此实际目录项指针 = 目录项号<<2，也即(from >> 20)，"与"
	// 上0xffc确保目录项指针范围有效。dir表示起始的页目录项物理地址
	dir = (unsigned long *) ((from >> 20) & 0xffc); 			/* _pg_dir = 0 */
	// 此时size是释放的页表个数，即页目录项数，而dir是起始目录项指针。现在开始循环操作页目录项，依次
	// 释放每个页表中的页表项。如果当前目录项无效(P位=0)，表示该目录项没有使用(对应的页表不存在)，则
	// 继续处理下一个页表项。否则从目录项中取出页表地址pg_table，并对该页表中的1024个表项进行处理，
	// 释放有效页表(P位=1)对应的物理内存页面，或者从交换设备中释放无效页表项(P位=0)对应的页面，即释
	// 放交换设备中对应的内存页面(因为页面可能已经交换出去)。然后把该页表项清零，并继续处理下一页表项。
	// 当一个页表所有表项都处理完毕就释放该页表自身占据的内存页面，并继续处理下一页目录项。最后刷新页
	// 变换高速缓冲，并返回0。
	for ( ; size-- > 0 ; dir++) {
		// 如果该目录项不存在页表项，则直接跳过该页表项
		if (!(1 & *dir))
			continue;
		pg_table = (unsigned long *) (0xfffff000 & *dir);	/* 取页表地址 */
		for (nr = 0 ; nr < 1024 ; nr++) {
			/* 若所指页表项内容不为0。则若该项有效，则释放对应面 */
			if (*pg_table) {
				if (1 & *pg_table)
					free_page(0xfffff000 & *pg_table);
				else										/* 否则释放交换设备中对应页 */
					swap_free(*pg_table >> 1);
				*pg_table = 0;								/* 该页表项内容清零 */
			}
			pg_table++;										/* 指向页表中下一项 */
		}
		free_page(0xfffff000 & *dir);						/* 释放该页表所占内存页面 */
		*dir = 0;											/* 对应页表的目录项清零 */
	}
	invalidate();											/* 刷新CPU页变换高速缓冲 */
	return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
/*
 * 好了，下面是内存管理mm中最为复杂的程序之一。它通过只复制内存页面来复制一定范围内线性地址中的内
 * 容。希望代码中没有错误，因为我不想再调试这块代码了:-)
 *
 * 注意！我们并不复制任何内存块，内存块的地址需要是4MB的倍数(正好一个页目录项对应的内存长度)，因
 * 为这样处理可使函数简单。不管怎样，它仅被fork()使用。
 *
 * 注意2！！当from==0时，说明是在为第一次fork()调用复制内核空间。此时我们就不想复制整个页目录项
 * 对应的内存，因为这样做会导致内存严重浪费我们只须复制开头160个页面，对应640KB。即使是复制这些页
 * 面也已经超出我们的需求,但这不会占用更多的内存,在低1MB内存范围内不执行写时复制操作，所以这些页面
 * 可以与内核共享。因此这是nr=xxxx的特殊情况(nr在程序中指页面数)。
 */
// 复制目录表项和页表项。
// 复制指定线性地址和长度内存对应的页目录项和页表项，从而被复制的页目录和页表对应的原物理内存页面区
// 被两套页表映射而共享使用。复制时，需申请新页面来存放新页表，原物理内存区将被共享。此后两个进程(父
// 进程和其子进程)将共享内存区，直到有一个进程执行写操作时，内核才会为写操作进程分配新的内存页(写时
// 复制机制)。
// 参数from、to是线性地址，size是需要复制(共享)的内存长度，单位是字节。
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long new_page;
	unsigned long nr;

	// 首先检测参数给出的源地址from和目的地址to的有效性。源地址和目的地址都需要在4MB内存边界地址上。
	// 否则出错死机。作这样的要求是因为一个页表的1024项可管理4MB内存。源地址from和目的地址to只有满
	// 足这个要求才能保证从一个页表的第1项开始复制页表项，并且新页表最初所有项都是有效的。然后取得源
	// 地址和目的地址的起始目录项指针(from_dir和do_dir)。再根据参数给出的长度size计算要复制的内存
	// 块占用的页表数(即目录项数)
	if ((from & 0x3fffff) || (to & 0x3fffff))
		panic("copy_page_tables called with wrong alignment");
	from_dir = (unsigned long *) ((from >> 20) & 0xffc); 			/* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to >> 20) & 0xffc);
	size = ((unsigned) (size + 0x3fffff)) >> 22;
	// 在得到了源起始目录项指针from_dir和目的起始目录项指针to_dir以及需要复制的页表个数size后，
	// 下面开始对每个页目录项依次申请1页内存来保存对应的面表，并且开始页表项复制操作。如果目的目录项
	// 指定的页表已经存在(P=1)，则出错死机。如果源目录项无效，即指定的页表不存在(P=0)，则继续循环处
	// 理下一个页目录项。
	for( ; size-- > 0 ; from_dir++, to_dir++) {
		if (1 & *to_dir)
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))
			continue;
		// 在验证了当前源目录项和目的项正常之后，取源目录项中页表地址from_page_table。为了保存目的
		// 目录项对应的页表，需要在主内存区中申请1页空闲内存页。如果取空闲页面函数get_free_page()返
		// 回0，则说明没有申请到空闲内存页面，可能是内存不够，于是返回-1值退出。
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page()))
			return -1;							/* Out of memory, see freeing */
		// 否则我们设置目的目录项信息，把最后3位置位，即当前目的目录项"或"上7，表示对应页表映射的内存
		// 页面是用户级的，并且可读写，存在(User，R/W，Present)。(如果U/S位是0，则R/W就没有作用。
		// 如果U/S是1，而R/W是0，那么运行在用户层的代码就只能读页面。如果U/S和R/W都置位，则就有读写
		// 的权限)。
		*to_dir = ((unsigned long) to_page_table) | 7;
		// 然后针对当前处理的页目录项对应的页表，设置需要复制的页面项数。如果是在内核空间，则仅需复制
		// 头160页对应的页表项(nr = 160)，对应于开始640KB物理内存。否则需要复制一个页表中的所有1024
		// 个页表项(nr= 1024)，可映射4MB物理内存。
		nr = (from == 0) ? 0xA0 : 1024;
		// 此时对于当前页表，开始循环复制指定的nr个内存页面表项。先取出源页表项内容，如果当前源页面没有
		// 使用(项内容为0)，则不用复制该表项，继续处理下一项。
		for ( ; nr-- > 0 ; from_page_table++, to_page_table++) {
			this_page = *from_page_table;
			// 如果源页表不存在，则直接拷贝下一页表
			if (!this_page)
				continue;
			// 如果该表项有内容，但是其存在位P=0，则该表项对应的页面可能在交换设备中。于是先申请1页内
			// 存，并从交换设备中读入该页面(若交换设备中有的话)。然后将该页表项复制到目的页表项中。并
			// 修改源页表项内容指向该新申请的内存页。
			if (!(1 & this_page)) {
				// 申请一页新的内存然后将交换设备中的数据读取到该页面中
				if (!(new_page = get_free_page()))
					return -1;
				// 从交换设备中将页面读取出来
				read_swap_page(this_page >> 1, (char *) new_page);
				// 目的页表项指向源页表项值
				*to_page_table = this_page;
				// 并修改源页表项内容指向该新申请的内存页，并设置表项标志为"页面脏"加上7
				*from_page_table = new_page | (PAGE_DIRTY | 7);
				// 继续处理下一页表项
				continue;
			}
			// 复位页表项中R/W标志(位1置0)，即让页表项对应的内存页面只读，然后将该页表项复制到目的页表中
			this_page &= ~2;
			*to_page_table = this_page;
			// 如果该页表项所指物理页面的地址在1MB以上，则需要设置内存页面映射数组mem_map[]，于是
			// 计算页面号，并以它为索引在页面同数组相应项中增加引用次数。而对于位于1MB以下的页面，说
			// 明是内核页面，因此不需要对mem_map[]进行设置。因为mem_map[]仅用于管理主内存区中的页
			// 面使用请问。因此对于内核移动到任务0中并且调用fork()创建任务1时(用于运行init())，由
			// 于此时复制的页面还仍然都在内核代码区域，因此以下判断中的语句不会执行，任务0的页面仍然
			// 可以随时读写。只有当调用fork()的父进程代码处于主内存(页面位置大于1MB)时才会执行。这
			// 种情况需要在进程调用execve()，并装载执行了新程序代码时才会出现。157行语句含义是令源
			// 页表项所指内存页也为只读。因为现在开始已有两个进程共用内存区了。若其中1个进程需要进行
			// 操作，则可以通过页异常写保护处理为执行写操作的进程分配1页新空闲页面，也即进行写时复制
			// (copy_on_write)操作。
			if (this_page > LOW_MEM) {
				*from_page_table = this_page;	/* 令源页表项也只读 */
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
        }
    }
	invalidate();								/* 刷新页变换高速缓冲 */
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
/*
 * 下面函数将一内存页面放置(映射)到指定线性地址处。它返回页面的物理地址，如果内存不够(在访问页面或页
 * 面时)，则返回0。
 */
// 把一物理内存页面映射到线性地址空间指定处。
// 或者说是把线性地址空间中指定地址address处的页面映射到主内存区页面page上。主要工作是在相关页目录
// 项和页表项中设置指定页面的信息.若成功则返回物理页面地址。在处理缺页异常的C函数do_no_page()中会
// 调用此函数。对于缺页引起的异常，由于任何缺页缘故而对页表作修改时，并不需要刷新CPU的页变换缓冲(或
// 称Translation Lookaside Buffer，TLB)，即使页表项中标志P被从0修改成1。因为无效页项不会被缓
// 冲，因此当修改了一个无效的页表项时不需要刷新。在此就表现为不用调用Invalidate()函数。参数page是
// 分配的主内存区中某一页面(页帧,页框)的指针；address是线性地址。
static unsigned long put_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */
	/* 注意!!!这里使用了页目录表基地址pg_dir=0的条件 */

	// 首先判断参数给定物理内存页面page的有效性。如果该页面位置低于LOW_MEM(1MB)或超出系统实际含
	// 有内存高端HIGH_MEMORY，则发出警告。LOW_MEM是主内存区可能有的最小起始位置。当系统后果内存
	// 小于或等于6MB时，主内存区始于LOW_MEM处。再查看一下该page页面是不已经申请的页面，即判断其
	// 在内存页面映射字节图mem_map[]中相应字节是否以置位。若没有则需发出警告。
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[(page - LOW_MEM) >> 12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);
	// 然后根据参数指定的线性地址address计算其在页目录表中对应的目录项指针，并从中取得一级页表地址。
	// 如果该目录项有效(P=1)，即指定的页表在内存中，则从中取得指定页表地址放到page_table变量中。
	// 否则申请一空闲页面给页表使用，并在对应目录项中置相应标志(7 - User，U/S，R/W)。然后将该页表
	// 地址放到page_table变量中。
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7;
		page_table = (unsigned long *) tmp;
	}
	// 最后在找到的页表page_table中设置相关页表项内容，即把物理页面page的地址填入表项同时置位3个标
	// 志(U/S，W/R，P)。该页表项在页表中的索引值等于线性地址位21~位12组成的10位的值。每个页表共可
	// 有1024项(0~0x3ff)。
	page_table[(address >> 12) & 0x3ff] = page | 7;
	/* no need for invalidate */
	/* 不需要刷新页变换高速缓冲 */
	return page;			/* 返回物理页面地址 */
}

/*
 * The previous function doesn't work very well if you also want to mark
 * the page dirty: exec.c wants this, as it has earlier changed the page,
 * and we want the dirty-status to be correct (for VM). Thus the same
 * routine, but this time we mark it dirty too.
 */
/*
 * 如果你也想设置页面已修改标志，则上一个函数工作得不是很好:exec.c程序需要这种设置。因为exec.c中函数
 * 会在放置页面之前修改过页面内容。为了实现VM，我们需要能正确设置已修改状态标志。因而下面就有了与上面
 * 相同的函数，但是该函数在放置页面时会把页面标志为已修改状态。
 */
// 把一内容已修改过的物理内存页面映射到线性地址空间指定处
// 该函数与一个函数put_page()几乎完全一样，除了本函数在第223行设置页表项内容时，同时还设置了页面已修改
// 标志(位6,PAGE_DIRTY).
unsigned long put_dirty_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */

	// 首先判断参数给定物理内存页面page的有效性。如果该页面位置低于LOW_MEM(1MB)或超出系统实际含有
	// 内存高端HIGH_MEMORY,则发出警告。LOW_MEM是主内存区可能有的最小起始位置。当系统后果内存小于
	// 或等于6MB时，主内存区始于LOW_MEM处。再查看一下该page页面是不已经申请的页面，即判断其在内存
	// 页面映射字节图mem_map[]中相应字节是否以置位。若没有则需发出警告。
	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);
	// 然后根据参数指定的线性地址address计算其在页目录表中对应的目录项指针，并从中取得一级页表地址。
	// 如果该目录项有效(P=1)，即指定的页表在内存中，则从中取得指定页表地址放到page_table变量中。
	// 否则申请一空闲页面给页表使用，并在对应目录项中置相应标志(7 - User，U/S，R/W)。然后将该页
	// 表地址放到page_table变量中。
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7;
		page_table = (unsigned long *) tmp;
	}
	// 最后在找到的页表page_table中设置相关页表项内容，即把物理页面page的地址填入表项同时
	// 置位3个标志(U/S，W/R，P)。该页表项在页表中的索引值等于线性地址位21~位12组成的10位
	// 的值。每个页表共可有1024项(0~0x3ff)。
	page_table[(address >> 12) & 0x3ff] = page | (PAGE_DIRTY | 7);
	/* no need for invalidate */
	/* 不需要刷新页变换高速缓冲 */
	return page;
}

// 取消写保护页面函数
// 用于页异常中断过程中写保护异常的处理(写时复制)。在内核创建进程时，新进程与父进程被设置成
// 共享代码和数据内存页面，并且所有这些页面均被设置成只读页面。而当新进程或原进程需要向内存页
// 面写数据时，CPU就会检测到这个情况并产生页面写保护异常。于是在这个函数中内核就会首先判断要
// 写的页面是否被共享。若没有则把页面设置成可写然后退出。若页面处于共享状态，则要重新申请一新
// 页面并复制被写页面内容，以供写进程单独使用。共享被取消。输入参数为页面表项指针，是物理地
// 址。[un_wp_page -- Un-Write Protect Page]
void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page, new_page;

	// 首先取参数指定的页表项中物理页面位置(地址)并判断该页面是不是共享页面。如果原页面地址大
	// 于内存低端LOW_MEM(表示在主内存区中)，并且其在页面映射字节图数组中值为1(表示页面仅被引
	// 用1次，页面没有被共享)，则在该页面的页表项中 R/W标志(可写)，并刷新页变换高速缓冲,然后
	// 返回。即如果该内存页面此时只被一个进程使用，并且不是内核中的进程，就直接把属性改为可写
	// 即可，不必重新申请一个新页面。
	old_page = 0xfffff000 & *table_entry;/* 取指定页表项中物理页面地址 */
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	// 否则就需要在主内存区内申请一页空闲页面给执行写操作的进程单独使用，取消页面共享。如果原
	// 页面大于内存低端(则意味着mem_map[]>1，页面是共享的)，则将原页面的页面映射字节数组值
	// 递减1。然后将指定页表项内容更新为新页面地址，并置可读写标志(U/S，R/W，P)。在刷新页变
	// 换高速缓冲之后，最后将原页面内容复制到新页面。
	if (!(new_page = get_free_page()))
		oom();							/* 内存不够处理 */
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	copy_page(old_page, new_page);
	// 将新的页面设置为可读可写且存在
	*table_entry = new_page | 7;
	// 刷新高速缓冲
	invalidate();
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 */
/*
 * 当用户试图住一共享页面上写时，该函数处理已存在的内存页面(写时复制)，它是通过将页面复制到
 * 一个新地址上并且递减原页面的共享计数值实现的。
 *
 * 如果它在代码空间，我们就显示段出错信息并退出。
 */
// 执行写保护页面处理
// 是写共享页面处理函数。是页异常中断处理过程中调用的C函数。在page.s程序中被调用。
// 函数参数error_code和address是进程在写写保护页面时由CPU产生异常而自动生成的。error_code
// 指出出错类型；address是产生异常的页面线性地址。写共享页面时需复制页面(写时复制)。
void do_wp_page(unsigned long error_code, unsigned long address)
{
	// 首先判断CPU控制寄存器CR2给出的引起页面异常的线性地址在什么范围中。如果address小于
	// TASK_SIZE(0x4000000，即64MB)，表示异常页面位置在内核或任务0和任务1所处的线性地
	// 址范围内，于是发出警告信息"内核范围内存被写保护"；如果(address - 当前进程代码起始
	// 地址)大于一个进程的长度(64MB)，表示address所指的线性地址不在引起异常的进程线性地
	// 址空间范围内，则在发出出错信息后退出。
	if (address < TASK_SIZE)
		printk("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");
	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: page error in do_wp_page\n\r");
		do_exit(SIGSEGV);
	}
#if 0
	/* we cannot do this yet: the estdio library writes to code space */
	/* stupid, stupid. I really want the libc.a from GNU */
	/* 我们现在还不能这样做:因为estdio库会在代码空间执行写操作 */
	/* 真是太愚蠢了。我真想从GNU得到libca库。 */
	// 如果线性地址位于进程的代码空间中，则终止执行程序。因为代码是只读的。
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif
	// 调用上面函数un_wp_page()来处理取消页面保护。但首先需要为其准备好参数。参数是线性地
	// 址address指定页面在页表中的页表项指针，其计算方法是:
	// 1:((address>>10) & 0xffc):计算指定线性地址中页表项在页表中的偏移地址；因为根据线性
	// 地址结构，(address >> 12)就是页表项中索引，但每项占4个字节，因此乘4后:
	// (address>>12)<<2=(address>>10)&0xffc就可得到页表项在表中的偏移地址。与操作&0xffc
	// 用于限制地址范围在一个页面内。又因为只移动了10位，因此最后2位是线性地址低12位中的最高2
	// 位。也应屏蔽掉。因此求线性地址中页表项在页表中偏移地址直观一些的表示方法是
	// (((address>>12)&0x3ff)<<2)
	// 2:(0xfffff000&*((address>>20)&0xffc)):用于取目录项中页表的地址值；其中，
	// ((address>>20)&0xffc)用于取线性地址中的目录索引项在目录表中的偏移位置。因为address>>22
	// 是目录项索引值，但每项4个字节，因此乘以4后:(address>>22)<<2=(address>>20)就是指定项在
	// 目录表中的偏移地址。&0xffc用于屏蔽目录项索引值中最后2位。因为只移动了20位，因此最后2位是页
	// 表索引的内容，应该屏蔽掉。而*((address>>20)&0xffc)则是取指定目录表项内容中对应页表的物理
	// 地址。最后与上0xffffff000用于屏蔽掉页目录项内容中的一些标志位(目录项低12位)。直观表示为
	// (0xffffff000 & *((unsigned long *) (((address>>22) & 0x3ff)<<2)))。
	// 3:由1中页表项在页表中偏移地址加上2中目录表项内容中对应页表的物理地址即可得到页表项的指针(物
	// 理地址)。这里对共享的页面进行复制。
	un_wp_page((unsigned long *)
		(((address >> 10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address >> 20) & 0xffc)))));

}

/**
 * 写页面验证
 * 若页面不可写，则复制页面。在fork.c中被内存验证通用函数verify_area()调用
 * @param[in]	address		指定页面在4GB空间中的线性地址
 * @retval		void
 */
void write_verify(unsigned long address)
{
	unsigned long page;

	// 首先取指定线性地址对应的页目录项，根据目录项中的存在位(P)判断目录项对应的页表是否存在
	// (存在位P=1?)，若不存在(P=0)则返回。这样处理是因为对于不存在的页面没有共享和写时复制可
	// 言，并且若程序对此不存在的页面执行写操作时，系统就会因为缺页异常而去执行do_no_page()，
	// 并为这个地方使用put_page()函数映射一个物理页面。接着程序从目录项中取页表地址，加上指定
	// 页面在页表中的页表项偏移值，得对应地址的页表项指针。在该表项中包含着给定线性地址对应的物
	// 理页面。
	if (!( (page = *((unsigned long *) ((address >> 20) & 0xffc)) ) & 1))
		return;
	page &= 0xfffff000;
	// 得到页表项的物理地址
	page += ((address >> 10) & 0xffc);
	// 然后判断该页表项中位1(P/W)，位0(P)标志。如果该页面不可写(R/W=0)且存在，那么就执行共
	// 享检验和复制页面操作(写时复制)。否则什么也不做，直接退出。
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

// 取得一页空闲内存并映射到指定线性地址处。
// get_free_page()仅是申请取得了主内存区的一页物理内存。而本函数则不仅是获取到一页物理内存
// 页面，还进一步调用put_page()，将物理页面映射到指定的线性地址处。
// 参数address是指定页面的线性地址。
void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	// 若不能取得一空闲页面，或者不能将所取页面放置到指定地址处，则显示内存不够的信息。292行
	// 上英文注释的含义是:free_page()函数的参数tmp是0也没有关系，该函数会忽略它并能正常返回。
	if (!(tmp = get_free_page()) || !put_page(tmp, address)) {
		free_page(tmp);		/* 0 is ok - ignored */
		oom();
	}
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable or library.
 */
/*
 * tty_to_share()任务“p”中检查位于地址“address”处的页面，看页面是否存在，是否干净。如果
 * 干净的话，就与当前任务共享。
 *
 * 注意！这里我们已假定p!=当前任务，并且它们共享同一个执行程序或库程序。
 */
// 尝试对当前进程指定地址处的页面进行共享处理。
// 当前进程与进程p是同一执行代码，也可以认为当前进程是由p进程执行fork操作产生的进程，因此它们
// 的代码内容一样。如果未对数据段内容作过修改那么数据段内容也应一样。参数address是进程中的逻
// 辑地址，即是当前进程欲与p进程共享页面的逻辑页面地址。进程p是将被共享页面的进程。如果p进程
// address处的页面存在并且没有被修改过的话，就让当前进程与p进程共享之。同时还需要验证指定
// 的地址处是否已经申请了页面，若是则出错，死机。
// 返回：1 - 页面共享处理成功；0 - 失败。
static int try_to_share(unsigned long address, struct task_struct * p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	// 首先分别求得指定进程p中和当前进程中逻辑地址address对应的页目录项。为了计算方便先求出
	// 指定逻辑地址address处的“逻辑”页目录号，即以进程空间（0 - 64MB）算出的页目录项号。
	// 该“逻辑”页目录项号加上进程p在CPU 4GB线性空间中起始地址对应的页目录项，即得到进程p中
	// 地址address处页面所对应的4GB线性空间中实际页目录项from_page。而“逻辑”页目录项号加
	// 上当前进程CPU 4GB线性空间中的实际页目录项to_page。
	from_page = to_page = ((address >> 20) & 0xffc);
	from_page += ((p->start_code >> 20) & 0xffc);             		// p进程目录项。
	to_page += ((current->start_code >> 20) & 0xffc);         		// 当前进程目录项。
	// 在得到p进程和当前进程address对应的目录项后，下面分别对进程p和当前进程处理。首先对p进
	// 程的表项进行操作。目录是取得p进程中address对应的物理内在页面地址，并且该物理页面存在，
	// 而且干净（没有被修改过，不脏）。方法是先取目录项内容。如果该目录项元效（P=0），表示目
	// 录项对应的二级页表不存在，于是返回。否则取该目录项对应页表地址from，从而计算出逻辑地
	// 址address对应的页表项指针，并取出该面表项内容临时保存在phys_addr中。
	/* is there a page-directory at from? */
	/* 在from处是否存在页目录项？ */
	from = *(unsigned long *) from_page;         	/* p进程目录项内容 */
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;                        		/* 页表地址 */
	from_page = from + ((address >> 10) & 0xffc);	/* 页表项指针 */
	phys_addr = *(unsigned long *) from_page; 		/* 页表项内容 */
	// 接着看看页表项映射的物理页面是否存在并且干净。0x41对应页表项中的D（Dirty）和
	// P（present）标志。如果页面不干净或无效则返回。然后我们从该表项中取出物理页面地址再
	// 保存在phys_addr中。最后我们再检查一下这个物理页面地址的有效性，即它不应该超过机器
	// 最大物理地址值，也不应该小于内在低端（1MB）。
	/* is the page clean and present? */
	/* 物理页面干净并且存在吗？ */
	if ((phys_addr & 0x41) != 0x01)
		return 0;
	phys_addr &= 0xfffff000;					/* 物理页面地址 */
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
	// 下面首先对当前进程的表项进行操作。目标是取得当前进程中address对应的页表项地址，并且该页
	// 表项还没有映射物理页面，即其P=0。首先取当前进程页目录项内容->to。如果该目录项元效(P=0)，
	// 即目录项对应的二级页表不存在，则申请一空闲页面来存放页表，并更新目录项to_page内容，让其
	//指向该内存页面。
	to = *(unsigned long *) to_page;           	/* 当前进程目录项内容 */
	if (!(to & 1))
		if (to = get_free_page())
			*(unsigned long *) to_page = to | 7;
		else
			oom();
	// 否则取目录项中的页表地址->to，加上页表索引值<<2，即页表项在表中偏移地址，得到页表项
	// 地址->to_page。针对该页表项，如果此时我们检查出其对应的物理页面已经存在，即页表项的
	// 存在位P=1,则说明原本我们想共享进程p中对应的物理页面，但现在我们自己已经占有了（映射
	// 有）物理页面。于是说明内核出错，死机。
	to &= 0xfffff000;                         	/* 当前进程的页表地址 */
	to_page = to + ((address >> 10) & 0xffc); 	/* 当前进程的页表项地址 */
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
	/* share them: write-protect */
	/* 对它们进行共享处理：写保护区*/
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;
	// 随后刷新页变换高速缓冲。计算所操作物理页面的页面号，并将对应页面映射字节数组项中的引用
	//递增1。最后返回1，表示共享处理成功。
	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;                       	/* 得页面号 */
	mem_map[phys_addr]++;
	return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
/*
 * share_page()试图找到一个进程，它可以与当前进程共享页面。参数address是当前进程数据空间中
 * 期望共享的某页面地址。
 *
 * 首先我们通过检测executable->i_count来查证是否可行。如果有其他任务已共享该inode，则它应
 * 该大于1。
 */
// 共享页面处理。
// 在发生缺页异常时，首先看看能否与运行同一个执行文件的其他进程页面共享处理。该函数首先判断系统
// 中是否有另一个进程也在运行与当前进程一样的执行文件。若有,则在系统当前所有任务中寻找这样的任
// 务。若找到了这样的任务就尝试与其共享指定地址处的页面。若系统中没有其他任务正在运行与当前进程
// 相同的执行文件，那么共享页面操作的前提条件不存在，因此函数立刻退出。判断系统中是否有另一个进
// 程也在执行同一个执行文件的方法是利用进程任务数据结构中的executable字段(或library字段)。
// 该字段指向进程正在执行程序(或使用的库文件)在内存中的i节点。根据该i节点的引用次数i_count我们
// 可以进行这种判断。若节点的i_count值大于1，则表明系统中有两个进程正在运行同一个执行文件(或库
// 文件)，于是可以再对任务结构数组中所有任务比较是否有相同的executable字段(或library字段)来
// 最后确定多个进程运行着相同执行文件的情况。参数inode是欲进行共享页面进程执行文件的内存i节点。
// address是进程中的逻辑地址，即当前进程欲与p进程共享页面的逻辑页面地址。返回1 - 共享操作成功，
// 0 - 失败。
static int share_page(struct m_inode * inode, unsigned long address)
{
	struct task_struct ** p;

	// 首先检查一下参数指定的内存i节点引用计数值。如果该内存i节点的引用计数值等于1
	// (executalbe->i_count=1)或者i节点指针空，表示当前系统中只有1个进程在运行该执行文件或
	// 者提供的i节点无效。因此无共享可言,直接退出函数。
	if (inode->i_count < 2 || !inode)
		return 0;
	// 否则搜索任务数组中所有任务。寻找与当前进程可共享页面的进程，即运行相同执行文件的另一个进
	// 程，并尝试对指定地址的页面进行共享。若进程逻辑地址address小于进程库文件在逻辑地址空间的
	// 起始地址LIBRARY_OFFSET，则表明共享的页面在进程执行文件对应的逻辑地址空间范围内，于是
	// 检查一下指定i节点是否与进程的执行文件i节点(即进程executable相同，若不相同则继续寻找。若
	// 进程逻辑地址address大于等于进程库文件在逻辑地址空间的起始地址LIBRARY_OFFSET，则表明想
	// 要共享的页面在进程使用的库文件中，于是检查指定节点inode是否与进程的库文件i节点相同，若不
	// 相同则继续寻找。如果找到某个进程p，其executable或library与指定的节点inode相同，则调用
	// 页面试探函数try_to_share()尝试页面共享。若共享操作成功，则函数返回1。否则返回0，表示共
	// 享页面操作失败。
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)								/* 如果该任务项空闲，则继续寻找 */
			continue;
		if (current == *p)						/* 如果是当前任务，也继续寻找 */
			continue;
		if (address < LIBRARY_OFFSET) {
			if (inode != (*p)->executable)		/* 进程执行文件i节点 */
				continue;
		} else {
			if (inode != (*p)->library)			/* 进程使用库文件i节点 */
				continue;
		}
		if (try_to_share(address, *p))			/* 尝试共享页面 */
			return 1;
	}
	return 0;
}

// 执行缺页处理。
// 是访问不存在页面处理函数。页异常中断处理过程中调用的函数。在page.s程序中被调用。函数参数
// error_code和address是进程在访问页面时由CPU因缺页产生异常而自动生成。error_code指出
// 出错类型；address产生异常的页面线性地址。该函数首先查看所缺页是否在交换设备中，若是则交
// 换进来。否则尝试与已加载的相同文件进行页面共享，或者只是由于进程动态申请内存页面而只需映
// 射一页物理内存页即可。若共享操作不成功，那么只能从相应文件中读入所缺的数据页面到指定线性
// 地址处。
void do_no_page(unsigned long error_code, unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;
	struct m_inode * inode;

	// 首先判断CPU控制寄存器CR2给出的引起页面异常的线性地址在什么范围中。如果address小于
	// TASK_SIZE(0x4000000，即64MB)，表示异常页面位置在内核或任务0和任务1所处的线性地址
	// 范围内，于是发出警告信息"内核范围内存被写保护"；如果(address-当前进程代码起始地址)
	// 大于一个进程的长度(64MB)，表示address所指的线性地址不在引起异常的进程线性地址空间范
	// 围内，则在发出出错信息后退出。
	if (address < TASK_SIZE)
		printk("\n\rBAD!! KERNEL PAGE MISSING\n\r");
	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: nonexistent page error in do_no_page\n\r");
		do_exit(SIGSEGV);
	}
	// 然后根据指定的线性地址address求出其对应的二级页表项指针，并根据该页表项内容判断
	// address处的页面是否在交换设备中。若是则调入页面并退出。方法是首先取指定线性地址
	// address对应的目录项内容。如果对应的二级页表存在，则取出该目录项中二级页表的地址，加上页
	// 表项偏移值即得到线性地址address处页面对应的页表项指针，从而获得页表项内容。若页表内容不
	// 为0并且页表项存在位P=0，则说明该页表项指定的物理页面应该在交换设备中。于是从交换设备中
	// 调入指定页面后退出函数。
	page = *(unsigned long *) ((address >> 20) & 0xffc);	/* 取目录项内容 */
	if (page & 1) {
		page &= 0xfffff000;									/* 二级页表地址 */
		page += (address >> 10) & 0xffc;					/* 页表项指针 */
		tmp = *(unsigned long *) page;						/* 页表项内容 */
		if (tmp && !(1 & tmp)) {
			swap_in((unsigned long *) page);				/* 从交换设备读页面 */
			return;
		}
	}
	// 否则取线性空间中指定地址address处页面地址，并算出指定线性地址在进程空间中相对于进程基址的
	// 偏移长度值tmp，即对应的逻辑地址。从而可以算出缺页页面在执行文件映像中或在库文件中的具体起
	// 始数据块号。
	address &= 0xfffff000;						/* address处缺页页面地址 */
	tmp = address - current->start_code;		/* 缺页页面对应逻辑地址 */
	// 如果缺页对应的逻辑地址tmp大于库映像文件在进程逻辑空间中的起始位置，说明缺少的页面在库映
	// 像文件中。于是从当前进程任务数据结构中可以取得库映像文件的i节点library，并计算出该缺页
	// 在库文件中的起始数据块号block。因为设置上存放的执行文件映像第1块数据是程序头结构，因此
	// 在读取该文件时需要跳过第1块数据。所以需要首先计算缺页所在数据块号。因为每块数据长度为
	// BLOCK_SIZE = 1KB，因此一页内存可存放4个数据块。进程逻辑地址tmp除以数据块大小再加1即可
	// 得出缺少的页面在执行映像文件中的起始块号block。
	if (tmp >= LIBRARY_OFFSET ) {
		inode = current->library;				/* 库文件i节点和缺页起始块号 */
		block = 1 + (tmp - LIBRARY_OFFSET) / BLOCK_SIZE;
	// 如果缺页对应的逻辑地址tmp小于进程的执行映像文件在逻辑地址空间的末端位置，则说明缺少的页面
	// 在进程执行文件映像中，于是可以从当前进程任务数据机构中取得执行文件的i节点号executable，
	// 并计算出该缺页在执行文件映像中的起始数据块号block。若逻辑地址tmp既不在执行文件映像的地址
	// 范围内。
	} else if (tmp < current->end_data) {
		inode = current->executable;			/* 执行文件i节点和缺页起始块号 */
		block = 1 + tmp / BLOCK_SIZE;
	// 也不在库文件空间范围内，则说明缺页是进程访问动态申请的内存页面数据所致，因此没有对应i节
	// 点和数据块号(都置空)。
	} else {
		inode = NULL;							/* 是动态申请的数据或栈内存页面 */
		block = 0;
	}
	// 若是进程访问其动态申请的页面或为了存放栈信息而引起的缺页异常，则直接申请一页物理内存页面
	// 并映射到线性地址address处即可。
	if (!inode) {								/* 是动态申请的数据内存页面 */
		get_empty_page(address);
		return;
	}
	// 否则说明所缺页面进程执行文件或库文件范围内，于是就尝试共享页面操作，若成功则退出。
	if (share_page(inode, tmp))					/* 尝试逻辑地址tmp处页面的共享 */
		return;
	// 如果共享不成功就只能申请一页物理内存页面page，然后从设备上读取执行文件中的相应页面并放置(映
	// 射)到进程页面逻辑地址tmp处。
	if (!(page = get_free_page()))				/* 申请一页物理内存 */
		oom();
	/* remember that 1 block is used for header */
	/* 记住，(程序)头要使用1个数据块 */
	// 根据这个块号和执行文件的i节点，我们就可以从映射位图中找到对应块设备中对应的设备逻辑块号(保存
	// 在nr[]数组中)。利用break_page()即可把这4个逻辑块读入到物理页面page中。
	for (i = 0 ; i < 4 ; block++, i++)
		nr[i] = bmap(inode, block);
	bread_page(page, inode->i_dev, nr);
	// 在读设备逻辑块操作时，可能会出现这样一种情况，即在执行文件中的读取页面位置可能离文件尾不到1个页
	// 面的长度。因此就可能读入一些无用的信息。下面的操作就是把这部分超出执行文件end_data以后的部分进
	// 行清零处理。当然，若该页面离末端超过1页，说明不是从执行文件映像中读取的页面，而是从库文件中读取
	// 的，因此不用执行清零操作。
	i = tmp + 4096 - current->end_data;			/* 超出的字节长度值 */
	if (i > 4095)								/* 离末端超过1页则不用清零 */
		i = 0;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;									/* tmp指向页面末端 */
		*(char *)tmp = 0;       				/* 页面末端i字节清零 */
	}
	// 最后把引起缺页异常的一页物理页面映射到指定线性地址address处。若操作成功就返回。否则就释放内
	// 存页，显示内存不够。
	if (put_page(page, address))
		return;
	free_page(page);
	oom();
}


/**
 * 物理内存管理初始化
 * 该函数对1MB以上内存区域以页面为单位进行管理前的初始化设置工作。一个页面长度为4KB。该函数1MB以上所
 * 有物理内存划分成一个个页面，并使用一个页面映射字节数组mem_map[]来管理所有这些页面。
 */
void mem_init(long start_mem, long end_mem)
{
	int i;

	/* 将1MB到16MB范围内的内存页面对应的数组项置为USED(100)，即已占用状态。
	 其中 PAGING_PAGES = PAGING_MEMORY>>12 = 15MB/4KB = 3840。 */
	HIGH_MEMORY = end_mem;			/* 设置内存最高端 */
	for (i = 0; i < PAGING_PAGES; i++) {
		mem_map[i] = USED;
	}

	/* 找到主内存区起始位置处页面号i */
	i = MAP_NR(start_mem);			/* i = (start_mem - 1MB) >> 12 */ 

	/* 得到主内存区的页面的数量 */								
	end_mem -= start_mem;
	end_mem >>= 12;

	/* 将主内存区对应的页面的使用数置0，即未使用 */
	while (end_mem-- > 0) {
		mem_map[i++] = 0;
	}
}



/**
 * 显示系统内存信息
 * 根据内存映射字节数组mem_map[]中的信息以及页目录和页表内容统计系统中使用的内存页面数和主内
 * 存区中总物理内存页面数。该函数在chr_drv/keyboard.S程序被调用，即当按下"Shift + Scroll Lock"
 * 组合键时会显示系统内存统计信息。
 */
void show_mem(void)
{
	int i, j, k, free = 0, total = 0;
	int shared = 0;
	unsigned long * pg_tbl;

	/* 根据内存映射字节数组mem_map[]，统计系统主内存区页面总数total，以及其中空闲页面数free 
	和被共享的页面数shared，并显示这些信息。*/
	printk("Mem-info:\n\r");
	for (i = 0 ; i < PAGING_PAGES ; i++) {
		if (mem_map[i] == USED) {		/* 1MB 以上内存系统占用的页面 */
			continue;
		}
		/* 统计主内存中的页面数 */
		total ++;
		if (!mem_map[i]) {
			/* 统计未使用的主内存页面数 */
			free ++;					/* 主内存区空闲页面统计 */
		}
		else {
			/* 统计共享页面数 */
			shared += mem_map[i] - 1;	/* 共享的页面数(字节值>1) */
		}
	}
	printk("%d free pages of %d\n\r", free, total);
	printk("%d pages shared\n\r", shared);
	// 统计处理器分页管理逻辑页面数。页目录表前4项供内核代码使用，不列为统计范围，因此扫描处理的页
	// 目录项从第5项开始。方法是循环处理所有页目录项(除前4个项)，若对应的二级页表存在，那么先统计
	// 二级页表本身占用的内存页面，然后对该页表中所有页表项对应页面情况进行统计。
	k = 0;								/* 一个进程占用页面统计值 */
	for (i = 4; i < 1024; ) {
		if (1 & pg_dir[i]) {
			// (如果页目录项对应二级页表地址大于机器最高物理内存地址 HIGH_MEMORY，说明该目录项有
			// 问题。于是显示该目录项信息并继续处理下一个目录项。
			if (pg_dir[i] > HIGH_MEMORY) {	/* 目录项内容不正常 */
				printk("page directory[%d]: %08X\n\r", i, pg_dir[i]);
				continue;
			}
			// 如果页目录项对应二级页表的"地址"大于LOW_MEM(即1MB)，则把一个进程占用的物理内存页
			// 统计值k增1，把系统占用的所有物理内存页统计值free增1。然后邓对应页表地址pg_tb1，并
			// 对该页表中所有页表项进行统计。如果当前页表项所指物理页面存在并且该物理页面"地址"大
			// 于LOW_MEME，那么就将页表项对应页面纳入统计值。
			if (pg_dir[i] > LOW_MEM) {
				free ++, k ++;				/* 统计页表占用页面 */
			}
			pg_tbl = (unsigned long *) (0xfffff000 & pg_dir[i]);
			for (j = 0 ; j < 1024 ; j++) {
				if ((pg_tbl[j]&1) && pg_tbl[j] > LOW_MEM){
					// (若该物理页面地址大于机器最高物理内存地址HIGH_MEMORY，则说明该页表项内
					// 容有问题，于是显示该页表项内容。否则将页表项对应页面纳入统计值。)
					if (pg_tbl[j] > HIGH_MEMORY){
						printk("page_dir[%d][%d]: %08X\n\r", i, j, pg_tbl[j]);
					}
					else{
						k ++, free ++;		/* 统计责表项对应页面 */
					}
				}
			}
		}
		// 因每个任务线性空间长度是64MB，所以一个任务占用16个目录项。因此这每统计了16个目录项就把
		// 进程的任务结构占用的页表统计进来。若此时 k=0 则表示当前的16个页目录所对应的进程在系统中
		// 不存在(没有创建或者已经终止)。在显示了对应进程号和其占用的物理内存页统计值k后，将k清零，
		// 以用于统计下一个进程占用的内存页面数。
		i++;
		if (!(i & 15) && k) {
			k ++, free ++;					/* one page/process for task_struct */
			printk("Process %d: %d pages\n\r", (i >> 4) - 1, k);
			k = 0;
		}
	}
	printk("Memory found: %d (%d)\n\r\n\r", free - shared, total);
}
