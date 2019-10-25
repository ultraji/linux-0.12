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

/* 用于判断给定线性地址是否位于当前进程的代码段中，“(((addr)+4095)&~4095)”用于取得线性
 地址addr所在内存页面的末端地址 */
#define CODE_SPACE(addr)	((((addr) + 4095) & ~4095) < current->start_code + current->end_code)

/* 存放实际物理内存最高端地址 */
unsigned long HIGH_MEMORY = 0;	

/* 从from处复制一页内存到to处(4KB) */
#define copy_page(from, to) 	__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

/* 内存映射字节图(1字节代表1页物理内存的使用情况) */
unsigned char mem_map [ PAGING_PAGES ] = {0, };

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */

/**
 * 释放物理地址addr开始的1页内存
 * @param[in]	addr	需要释放的起始物理地址
 * @return		void
 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM) {
		return;
	}
	if (addr >= HIGH_MEMORY) {
		panic("trying to free nonexistent page");
	}
	/* 页面号 = (addr-LOW_MEM)/4096 */
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--) {
		return;
	}
	/* 执行到此处表示要释放原本已经空闲的页面，内核存在问题 */
	mem_map[addr] = 0;
	panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
/*
 * 下面函数释放页表连续的内存块，exit()需要该函数。与copy_page_tables()类似，该函数仅处理
 * 4MB长度的内存块。
 */

/**
 * 根据指定的线性地址和限长(页表个数)，释放指定页面
 * @param[in]	from		起始线性基地址
 * @param[in] 	size		释放的字节长度
 * @return		0
 */
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	if (from & 0x3fffff) {	/* 参数from给出的线性基地址是否在4MB的边界处 */
		panic("free_page_tables called with wrong alignment");
	}
	if (!from) {			/* from=0说明试图释放内核和缓冲所在的物理内存空间 */
		panic("Trying to free up swapper memory space");
	}
	/* 计算size指定长度所占的页目录数（4MB的进位整数倍，向上取整），例如size=4.01MB则size=2 */
	size = (size + 0x3fffff) >> 22;
	/* 页目录项指针 */
	dir = (unsigned long *) ((from >> 20) & 0xffc); 			/* _pg_dir = 0 */
	/* 遍历需要释放的页目录项，释放对应页表中的页表项 */
	for ( ; size-- > 0 ; dir++) {
		if (!(1 & *dir)) {
			continue;
		}
		pg_table = (unsigned long *) (0xfffff000 & *dir);
		for (nr = 0 ; nr < 1024 ; nr++) {
			if (*pg_table) {
				if (1 & *pg_table) {	/* 在物理内存中  */
					free_page(0xfffff000 & *pg_table);
				} else {				/* 在交换设备中 */
					swap_free(*pg_table >> 1);
				}
				*pg_table = 0;
			}
			pg_table++;
		}
		free_page(0xfffff000 & *dir);
		*dir = 0;
	}
	invalidate();
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
/**
 * 复制目录表项和页表项（用于写时复制机制）
 * 复制指定线性地址和长度内存对应的页目录项和页表项，从而被复制的页目录和页表对应的原物理内存页面区
 * 被两套页表映射而共享使用。复制时，需申请新页面来存放新页表，原物理内存区将被共享。此后两个进程(父
 * 进程和其子进程)将共享内存区，直到有一个进程执行写操作时，内核才会为写操作进程分配新的内存页。
 * @param[in]	from	源线性地址
 * @param[in]	to		目标线性地址
 * @param[in]	size	需要复制的长度(单位是字节)
 * @return		0
 */
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long new_page;
	unsigned long nr;

	/* 源地址和目的地址都需要在4MB内存边界地址 */
	if ((from & 0x3fffff) || (to & 0x3fffff)) {
		panic("copy_page_tables called with wrong alignment");
	}
	/* 源地址的目录项指针，目标地址的目录项指针， 需要复制的目录项数 */
	from_dir = (unsigned long *) ((from >> 20) & 0xffc); 	/* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to >> 20) & 0xffc);
	size = ((unsigned) (size + 0x3fffff)) >> 22;
	/* 开始页表项复制操作 */
	for( ; size-- > 0 ; from_dir++, to_dir++) {
		if (1 & *to_dir) {
			panic("copy_page_tables: already exist");
		}
		if (!(1 & *from_dir)) {
			continue;
		}
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page())) {
			return -1;		/* Out of memory, see freeing */
		}
		*to_dir = ((unsigned long) to_page_table) | 7;
		/* 源地址在内核空间，则仅需复制前160页对应的页表项(nr = 160)，对应640KB内存 */
		nr = (from == 0) ? 0xA0 : 1024;
		/* 循环复制当前页表的nr个内存页面表项 */
		for ( ; nr-- > 0 ; from_page_table++, to_page_table++) {
			this_page = *from_page_table;
			if (!this_page) {
				continue;
			}
			/* 该页面在交换设备中，申请一页新的内存，然后将交换设备中的数据读取到该页面中 */
			if (!(1 & this_page)) {
				if (!(new_page = get_free_page())) {
					return -1;
				}
				read_swap_page(this_page >> 1, (char *) new_page);
				*to_page_table = this_page;
				*from_page_table = new_page | (PAGE_DIRTY | 7);
				continue;
			}
			this_page &= ~2;	/* 让页表项对应的内存页面只读 */
			*to_page_table = this_page;
			/* 物理页面的地址在1MB以上，则需在mem_map[]中增加对应页面的引用次数 */
			if (this_page > LOW_MEM) {
				*from_page_table = this_page;	/* 令源页表项也只读 */
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
        }
    }
	invalidate();
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */

/**
 * 将一页物理内存页面page映射到指定线性地址address处
 * @param[in]	page	物理内存页面的地址
 * @param[in]	address	指定线性地址
 * @retval		成功返回页面的物理地址，失败返回0
 */
static unsigned long put_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */
	/* 注意!!! 这里使用了页目录表基地址pg_dir=0的条件 */

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	/* page指向的页面未标记为已使用，故不能做映射 */
	if (mem_map[(page - LOW_MEM) >> 12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);

	/* 根据address从页目录表取出页表地址 */
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)	/* 页表存在 */
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7; 	/* 置位3个标志(U/S，W/R，P) */
		page_table = (unsigned long *) tmp;
	}
	/* 在页表中设置页面地址，并置位3个标志(U/S，W/R，P) */
	page_table[(address >> 12) & 0x3ff] = page | 7;

	/* no need for invalidate */
	return page;
}

/*
 * The previous function doesn't work very well if you also want to mark
 * the page dirty: exec.c wants this, as it has earlier changed the page,
 * and we want the dirty-status to be correct (for VM). Thus the same
 * routine, but this time we mark it dirty too.
 */
/*
 * 如果你也想设置页面已修改标志，则上一个函数工作得不是很好:exec.c程序需要这种设置。
 * 因为exec.c中函数会在放置页面之前修改过页面内容。为了实现VM，我们需要能正确设置已
 * 修改状态标志。因而下面就有了与上面相同的函数，但是该函数在放置页面时会把页面标志
 * 为已修改状态。
 */

/**
 * 把一内容已修改过的物理内存页面page映射到指定线性地址address处
 * @note		与上面的put_page函数一样，仅多设置了Dirty位
 * @param[in]	page	物理内存页面的地址
 * @param[in]	address	指定线性地址
 * @retval		成功返回页面的物理地址，失败返回0
 */
unsigned long put_dirty_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7;
		page_table = (unsigned long *) tmp;
	}
	page_table[(address >> 12) & 0x3ff] = page | (PAGE_DIRTY | 7);
	
	/* no need for invalidate */
	return page;
}

/**
 * 取消写保护页面函数	[un_wp_page -- Un-Write Protect Page]
 * 用于页异常中断过程中写保护异常的处理(写时复制)。在内核fork创建进程时，copy_mem将父子进程的
 * 页面均被设置成只读页面。而当新进程或原进程需要向内存页面写数据时，CPU就会检测到这个情况并
 * 产生页面写保护异常。于是在这个函数中内核就会首先判断要写的页面是否被共享。若没有则把页面设
 * 置成可写然后退出。若页面处于共享状态，则要重新申请一新页面并复制被写页面内容，以供写进程单
 * 独使用。共享被取消。
 * @param[in]	*table_entry	物理页面地址
 * @return		void
 */
/*static*/ void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page, new_page;

	old_page = 0xfffff000 & *table_entry;

	/* 即如果该内存页面此时只被一个进程使用，就直接把属性改为可写即可 */
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	
	/* 申请一页空闲页面给执行写操作的进程单独使用，取消页面共享。复制原页面的内容至新页面，
	将指定页表项值更新为新页面地址 */
	if (!(new_page = get_free_page()))
		oom();							/* 内存不够处理 */
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	copy_page(old_page, new_page);
	*table_entry = new_page | 7;
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
/** 
 * 执行写保护页面处理（在page.s程序中被调用）
 * 写共享页面时需复制页面(写时复制)。
 * @param[in]	error_code		出错类型（没有用到）
 * @param[in]	address			产生异常的页面的线性地址（CR2寄存器的值）
 * @return		void
 */
void do_wp_page(unsigned long error_code, unsigned long address)
{
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
	/* 如果线性地址位于进程的代码空间(只读)中，则终止执行程序 */
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif
	/* 根据线性地址计算物理页面地址 */
	un_wp_page((unsigned long *)
		(((address >> 10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address >> 20) & 0xffc)))));

}

/**
 * 写页面验证
 * 若页面不可写，则复制页面。在fork.c中被内存验证通用函数verify_area()调用
 * @param[in]	address		指定页面在4GB空间中的线性地址
 * @return		void
 */
void write_verify(unsigned long address)
{
	unsigned long page;

	/* 指定线性地址对应的页目录项是否存在 */
	if (!( (page = *((unsigned long *) ((address >> 20) & 0xffc)) ) & 1)) {
		return;
	}
	page &= 0xfffff000;
	/* 得到页表项的物理地址 */
	page += ((address >> 10) & 0xffc);
	/* 然后判断该页表项中位1(R/W)，位0(P)标志 */
	if ((3 & *(unsigned long *) page) == 1) {  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	}
	return;
}

/**
 * 取得一页空闲的物理内存并映射到指定线性地址处
 * @param[in]	address		指定页面的线性地址
 * @return		void
 */
void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	/* 若不能取得一空闲页面，或者不能将所取页面放置到指定地址处，则显示内存不够的信息 */
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
/**
 * 尝试对当前进程指定地址处的页面进行共享处理
 * 当前进程与进程p是同一执行代码，也可以认为当前进程是由p进程执行fork操作产生的进程，因此它们
 * 的代码内容一样。如果未对数据段内容作过修改那么数据段内容也应一样。
 * @param[in]	address		进程空间的逻辑地址
 * @param[in]	p			将被共享页面的进程
 * @return	页面共享处理成功返回1，失败返回0
 */
static int try_to_share(unsigned long address, struct task_struct * p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	from_page = to_page = ((address >> 20) & 0xffc);				/* 逻辑地址的目录项偏移 */
	from_page += ((p->start_code >> 20) & 0xffc);             		/* p进程目录项 */
	to_page += ((current->start_code >> 20) & 0xffc);         		/* 当前进程目录项 */
	/* is there a page-directory at from? */ /* 在from处是否存在页目录项？ */
	from = *(unsigned long *) from_page;
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;
	from_page = from + ((address >> 10) & 0xffc);		/* 页表项指针 */
	phys_addr = *(unsigned long *) from_page;			/* 页表项内容 */

	/* is the page clean and present? */
	if ((phys_addr & 0x41) != 0x01)		/* 物理页面干净并且存在吗？ */
		return 0;
	phys_addr &= 0xfffff000;			/* 物理页面地址 */
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;

	to = *(unsigned long *) to_page;	/* 当前进程目录项内容 */
	if (!(to & 1)) {
		if ((to = get_free_page())) {
			*(unsigned long *) to_page = to | 7;
		} else {
			oom();
		}
	}

	to &= 0xfffff000;
	to_page = to + ((address >> 10) & 0xffc);		/* 当前进程的页表项地址 */
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
	/* share them: write-protect */ /* 对它们进行共享处理：写保护 */
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;
	
	invalidate();
	/* 将对应物理页面的引用递增1 */
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;
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

static int share_page(struct m_inode * inode, unsigned long address)
{
	struct task_struct ** p;

	/*如果该内存i节点的引用计数值等于1或者i节点指针空，表示当前系统中只有1个进程在运行该执行文件
	 或者提供的i节点无效，因此无共享可言 */
	if (inode->i_count < 2 || !inode)
		return 0;
	/* 搜索任务数组中所有任务。寻找与当前进程可共享页面的进程，即运行相同执行文件的另一个进程 */
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)				/* 该任务项空闲 */
			continue;
		if (current == *p)		/* 如果是当前任务 */
			continue;
		if (address < LIBRARY_OFFSET) {
			if (inode != (*p)->executable)
				continue;
		} else {
			if (inode != (*p)->library)
				continue;
		}
		if (try_to_share(address, *p))			/* 尝试共享页面 */
			return 1;
	}
	return 0;
}

/**
 * 执行缺页处理（在page.s中被调用）
 * 函数参数error_code和address是进程在访问页面时由CPU因缺页产生异常而自动生成。
 * 1. 首先查看所缺页是否在交换设备中，若是则交换进来。
 * 2. 否则尝试与已加载的相同文件进行页面共享，或者只是由于进程动态申请内存页面而只需
 *    映射一页物理内存页即可。
 * 3. 若共享操作不成功，那么只能从相应文件中读入所缺的数据页面到指定线性地址处。
 * @param[in]	error_code	出错类型（没用到）
 * @param[in]	address		产生异常的页面线性地址(CR2寄存器的值)
 * @return		void
 */
void do_no_page(unsigned long error_code, unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;
	struct m_inode * inode;

	if (address < TASK_SIZE)
		printk("\n\rBAD!! KERNEL PAGE MISSING\n\r");

	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: nonexistent page error in do_no_page\n\r");
		do_exit(SIGSEGV);
	}
	/* 1.所缺页在交换设备中，从交换设备读页面 */
	page = *(unsigned long *) ((address >> 20) & 0xffc);	/* 取目录项内容 */
	if (page & 1) { /* 存在位P */
		page &= 0xfffff000;
		page += (address >> 10) & 0xffc;
		tmp = *(unsigned long *) page;						/* 取页表项内容 */
		if (tmp && !(1 & tmp)) {
			swap_in((unsigned long *) page);
			return;
		}
	}
	/* 算出address处缺页的页面地址在进程空间中的偏移长度值tmp，根据偏移值判定缺页所在进程空
	 间位置，获取i节点和块号，用于之后从文件中加载页面 */
	address &= 0xfffff000;
	tmp = address - current->start_code;
	if (tmp >= LIBRARY_OFFSET ) { 		/* 缺页在库映像文件中 */
		inode = current->library;
		block = 1 + (tmp - LIBRARY_OFFSET) / BLOCK_SIZE;
	} else if (tmp < current->end_data) { /* 缺页在执行映像文件中 */
		inode = current->executable;
		block = 1 + tmp / BLOCK_SIZE;
	} else { /* 缺页在动态申请的数据或栈内存页面，无i节点和块号 */
		inode = NULL;
		block = 0;
	}

	/* 2. 缺页为动态申请的内存页面，则直接申请一页物理内存页面并映射到线性地址address即可 */
	if (!inode) {
		get_empty_page(address);
		return;
	}
	
	/* 3. 缺页在进程执行文件或库文件范围内，尝试共享页面操作 */
	if (share_page(inode, tmp))
		return;
	
	/* 4. 共享不成功就只能申请一页物理内存页面page，然后读取执行文件中的相应页面并映射到逻辑地址tmp处 */
	if (!(page = get_free_page()))
		oom();
	/* remember that 1 block is used for header */
	/* 记住，程序头占用1个数据块（用于解释上面 block = 1+ ...） */
	for (i = 0 ; i < 4 ; block++, i++)
		nr[i] = bmap(inode, block); /* 获取设备逻辑块号 */
	bread_page(page, inode->i_dev, nr);

	/* 读取执行程序最后一页（实际不满一页），把超出end_data后的部分进行清零处理，若该页面离执行程序末端
	 超过1页，说明是从库文件中读取的，因此不用执行清零操作 */
	i = tmp + 4096 - current->end_data;
	if (i > 4095)
		i = 0;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	/* 把引起缺页异常的一页物理页面映射到指定线性地址address处 */
	if (put_page(page, address))
		return;
	/* 否则释放物理页面，显示内存不够 */
	free_page(page);
	oom();
}


/**
 * 物理内存管理初始化
 * 该函数对1MB以上内存区域以页面为单位进行管理前的初始化设置工作，一个页面长度为4KB，并使用一个页
 * 面映射字节数组mem_map[]来管理所有这些页面。
 * @param[in]	start_mem	主内存区的开始地址
 * @param[in]	end_mem		主内存区的结束地址
 * @return		void
 */
void mem_init(long start_mem, long end_mem)
{
	int i;

	/* 首先，将1MB到16MB范围内的内存页面对应的数组项置为USED(100)，即已占用状态 */
	HIGH_MEMORY = end_mem;			/* 设置内存最高端 */
	for (i = 0; i < PAGING_PAGES; i++) {
		mem_map[i] = USED;
	}

	/* 找到主内存区起始位置处页面号i */
	i = MAP_NR(start_mem);

	/* 得到主内存区的页面的数量 */								
	end_mem -= start_mem;
	end_mem >>= 12;

	/* 将主内存区对应的页面的使用数置0，即未使用 */
	while (end_mem-- > 0) {
		mem_map[i++] = 0;
	}
}



/* 显示系统内存信息（在chr_drv/keyboard.S中被调用） */
void show_mem(void)
{
	int i, j, k, free = 0, total = 0;
	int shared = 0;
	unsigned long * pg_tbl;

	/* 根据mem_map[]统计主内存区页面总数total，以及其中空闲页面数free和被共享的页面数shared */
	printk("Mem-info:\n\r");
	for (i = 0 ; i < PAGING_PAGES ; i++) {
		if (mem_map[i] == USED) { /* 不可用页面 */
			continue;
		}
		total ++;
		if (!mem_map[i]) {
			free ++;
		}
		else {
			shared += mem_map[i] - 1;/* 共享的页面数(字节值>1) */
		}
	}
	printk("%d free pages of %d\n\r", free, total);
	printk("%d pages shared\n\r", shared);
	/* 统计分页管理的逻辑页面数 */
	k = 0;		/* 一个进程占用页面统计值 */
	for (i = 4; i < 1024; ) { 	/*页目录表前4项供内核代码使用，不列为统计范围 */
		if (1 & pg_dir[i]) {
			if (pg_dir[i] > HIGH_MEMORY) {	/* 目录项内容不正常 */
				printk("page directory[%d]: %08X\n\r", i, pg_dir[i]);
				continue;
			}
			if (pg_dir[i] > LOW_MEM) {
				free ++, k ++;		/* 统计页表占用页面 */
			}
			pg_tbl = (unsigned long *) (0xfffff000 & pg_dir[i]);
			for (j = 0 ; j < 1024 ; j++) {
				if ((pg_tbl[j]&1) && pg_tbl[j] > LOW_MEM){
					if (pg_tbl[j] > HIGH_MEMORY){ /* 页表项内容不正常 */
						printk("page_dir[%d][%d]: %08X\n\r", i, j, pg_tbl[j]);
					}
					else{
						k ++, free ++;	/* 统计页表项对应页面 */
					}
				}
			}
		}
		i++;
		/* 每个任务线性空间长度是64MB，每统计了16个目录项就统计了一个任务占用的页表 */
		if (!(i & 15) && k) {	/* k=0说明对应的任务没有创建或者已经终止 */
			k ++, free ++;		/* one page/process for task_struct */
			printk("Process %d: %d pages\n\r", (i >> 4) - 1, k);
			k = 0;
		}
	}
	printk("Memory found: %d (%d)\n\r\n\r", free - shared, total);
}
