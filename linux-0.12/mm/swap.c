/*
 *  linux/mm/swap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This file should contain most things doing the swapping from/to disk.
 * Started 18.12.91
 */
/*
 * 本程序应该包括绝大部分执行内存交换的代码(从内存到磁盘或反之)。从91年12月18日开始编制。
 */

#include <string.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

/* 每个字节8位，因此1页(4096B)共有32768个位。若1个位对应1页内存，则最多可管理32768个页面，对应
 128MB内存容量 */
#define SWAP_BITS (4096 << 3)

// 位操作宏。通过给定不同的"op"，可定义对指定比特位进行测试，设置或清除三种操作。
// 参数addr是指定线性地址；nr是指定地址处开始的比特位偏移位。该宏把给定地址addr处第nr个比
// 特位的值放入进位标志，设置或复位该比特位并返回进位标志值(即原位值)。
// 第25行上第一个指令随"op"字符的不同而组合形成不同的指令:
// 当op=""时，就是指令bt - (Bit Test)测试并用原值设置进位位。
// 当op="s"时，就是指令bts - (Bit Test and Set)设置比特位值并用原来设置进位位。
// 当op="r"时，就是指令btr - (Bit Test and Reset)复位比特位值并原值设置进位位。
// 内嵌汇编代码把基地址(%2)和比特偏移值(%1)所指定的比特位值先保存到进位标志CF中，然后设置(复
// 位)该比特位。指令abcl是带进位位加，用于根据进位位CF设置操作数(%0)。如果CF=1则返回寄存器
// 值=1，否则返回寄存器值=0。
#define bitop(name, op) 								\
static inline int name(char * addr, unsigned int nr) 	\
{ 														\
    int __res; 											\
    __asm__ __volatile__("bt" op " %1, %2; adcl $0, %0" \
        :"=g" (__res) 									\
        :"r" (nr),"m" (*(addr)),"0" (0)); 				\
    return __res; 										\
}

/* 这里根据不同的op字符定义3个内嵌函数 */
bitop(bit, "")				/* bit(char * addr, unsigned int nr) */
bitop(setbit, "s")			/* setbit(char * addr, unsigned int nr) */
bitop(clrbit, "r")			/* clrbit(char * addr, unsigned int nr) */

static char * swap_bitmap = NULL;
int SWAP_DEV = 0;		/* 内核初始化时设置的交换设备号 */

/*
 * We never page the pages in task[0] - kernel memory.
 * We page all other pages.
 */
/*
 * 我们从不交换任务0(task[0])的页面，即不交换内核页面，我们只对其他页面进行交换操作。
 */
/* 第1个虚拟内存页面，即从任务0末端(64MB)处开始的虚拟内存页面 */
#define FIRST_VM_PAGE (TASK_SIZE >> 12)			/* = 64MB/4KB = 16384 */
#define LAST_VM_PAGE (1024 * 1024)				/* = 4GB/4KB = 1048576 4G对
                                                应的页数 */
#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE)	/* = 1032192(从0开始计)(用总
                                                的页面数减去第0个任务的页面数) */

/**
 * 申请1页交换页面
 * 扫描整个交换映射位图(除对应位图本身的位0以外)，返回值为1的第一个比特位号，即目前空闲的
 * 交换页面号。
 * @param[in]   void
 * @retval      成功返回交换页面号，失败返回0
 */
static int get_swap_page(void)
{
    int nr;

    if (!swap_bitmap) {
        return 0;
    }
    for (nr = 1; nr < SWAP_BITS; nr++) {
        if (clrbit(swap_bitmap, nr)) {
            return nr;		/* 返回目前空闲的交换页面号 */
        }
    }
    return 0;
}

/**
 * 释放交换设备中指定的交换页面
 * 在交换位图中设置指定页面号对应的位(置1)。若原来该位就等于1，则表示交换设备中原来该页面就没
 * 有被占用，或者位图出错。于是显示出错信息并返回。参数指定交换页面号。
 * @param[in]	swap_nr
 * @retval		void
 */
void swap_free(int swap_nr)
{
    if (!swap_nr) {
        return;
    }
    if (swap_bitmap && swap_nr < SWAP_BITS) {
        if (!setbit(swap_bitmap, swap_nr)) {
            return;
        }
    }
    printk("Swap-space bad (swap_free())\n\r");
    return;
}

/**
 * 把指定页面交换进内存中
 * 把指定页表项的对应页面从交换设备中读入到新申请的内存页面中。修改交换位图中对应位(置位)，同
 * 时修改页表项内容，让它指向该内存页面，并设置相应标志。
 * @param[in]	table_ptr
 * @retval		void
 */
void swap_in(unsigned long *table_ptr)
{
    int swap_nr;
    unsigned long page;

    // 首先检查交换位图和参数有效性。如果交换位图不存在，或者指定页表项对应的页面已存在于内存
    // 中，或者交换页面号为0，则显示警告信息并退出。对于已放到交换设备中去的内存页面，相应页
    // 表项中存放的应是交换页面号*2，即(swap_nr << 1)。
    if (!swap_bitmap) {
        printk("Trying to swap in without swap bit-map");
        return;
    }
    if (1 & *table_ptr) {
        printk("trying to swap in present page\n\r");
        return;
    }
    swap_nr = *table_ptr >> 1;
    if (!swap_nr) {
        printk("No swap page in swap_in\n\r");
        return;
    }
    // 然后申请一页物理内存并从交换设备中读入页面号为swap_nr的页面。在把页面交换进来后，就把
    // 交换位图中对应比特位置位。如果其原本就是置位的，说明此次是再次从交换设备中读入相同的页
    // 面，于是显示一下警告信息。最后让页表指向该物理页面，并设置页面已修改，用户可读写和存在
    // 标志(Dirty,U/S,R/W,P)。
    if (!(page = get_free_page()))
        oom();
    read_swap_page(swap_nr, (char *) page);
    if (setbit(swap_bitmap, swap_nr))
        printk("swapping in multiply from same page\n\r");
    *table_ptr = page | (PAGE_DIRTY | 7);
}

// 尝试把页面交换出去
// 若页面没有被修改过则不必保存在交换设备中，因为对应页面还可以再直接从相应映像文件中读入。于是
// 可以直接释放掉相应物理页面了事。否则就申请一个交换页面号，然后把页面交换出去。此时交换页面号
// 要保存在对应页表项中，并且仍需要保持页表项存在位P=0。参数是页表项指针。页面换或释放成功返回1，
// 否则返回0。
int try_to_swap_out(unsigned long * table_ptr)
{
    unsigned long page;
    unsigned long swap_nr;

    // 首先判断参数的有效性。若需要交换出去的内存页面并不存在(或称无效)，则即可退出。若页表项
    // 指定的物理页面地址大于分页管理的内存高端PAGING_MEMORY(15MB)，也退出。
    page = *table_ptr;
    if (!(PAGE_PRESENT & page))
        return 0;
    if (page - LOW_MEM > PAGING_MEMORY)
        return 0;
    // 若内存页面已被修改过，但是该页面是被共享的，那么为了提高运行效率，此类页面不宜被交换出，
    // 于是直接退出，函数返回0。否则就申请一交换页面号，并把它保存在页表项中，然后把页面交换出
    // 去并释放对应物理内存页面.
    if (PAGE_DIRTY & page) {
        page &= 0xfffff000;					/* 取物理页面地址 */
        if (mem_map[MAP_NR(page)] != 1)
            return 0;
        if (!(swap_nr = get_swap_page()))	/* 申请交换页面号 */
            return 0;
        // 对于要交换设备中的页面，相应页表项中将存放的是(swap_nr << 1)。乘2(左移1位)是为
        // 了空出原来页表项的存在位(P)。只有存在位P=0并且页表项内容不为0的页面才会在交换设备
        // 中。Intel手册中明确指出，当一个表项的存在位P=0时(无效页表项)，所有其他位(位31-1)
        // 可供随意使用。下面写交换页函数write_swap_page(nr,buffer)被定义为
        // ll_rw_page(WRITE,SWAP_DEV,(nr),(buffer))。
        *table_ptr = swap_nr << 1;
        invalidate();						/* 刷新CPU页变换高速缓冲 */
        write_swap_page(swap_nr, (char *) page);
        free_page(page);
        return 1;
    }
    // 否则表明页面没有修改过.那么就不用交换出去,而直接释放即可.
    *table_ptr = 0;
    invalidate();
    free_page(page);
    return 1;
}

/*
 * Ok, this has a rather intricate logic - the idea is to make good
 * and fast machine code. If we didn't worry about that, things would
 * be easier.
 */
/*
 * OK,这个函数中有一个非常复杂的逻辑，用于产生逻辑性好并且速度快的机器码。如果我们不对此操心
 * 的话，那么事情可能更容易些。
 */
// 把内存页面放到交换设备中
// 从线性地址64MB对应的目录项(FIRST_VM_PAGE>>10)开始，搜索整个4GB线性空间，对有效页目录
// 二级页表指定的物理内存页面执行交换到交换设备中去的尝试。一旦成功地交换出一个页面，就返回-1。
// 否则返回0。该函数会在get_free_page()中被调用。
int swap_out(void)
{
    static int dir_entry = FIRST_VM_PAGE >> 10;	/* 即任务1的第1个目录项索引 */
    static int page_entry = -1;
    int counter = VM_PAGES;			/* 表示除去任务0以外的其他任务的所有页数目 */
    int pg_table;

    // 首先搜索页目录表，查找二级页表存在的页目录项pg_table。找到则退出循环，否则高速页目
    // 录项数对应剩余二级页表项数counter，然后继续检测下一项目录项。若全部搜索完还没有找到
    // 适合的(存在的)页目录项，就重新搜索。
    while (counter > 0) {
        pg_table = pg_dir[dir_entry];		/* 页目录项内容 */
        if (pg_table & 1)
            break;
        counter -= 1024;					/* 1个页表对应1024个页帧 */
        dir_entry++;						/* 下一目录项 */
        // 如果整个4GB的1024个页目录项检查完了则又回到第1个任务重新开始检查
        if (dir_entry >= 1024)
            dir_entry = FIRST_VM_PAGE >> 10;
    }
    // 在取得当前目录项的页表指针后，针对该页表中的所有1024个页面，逐一调用交换函数
    // try_to_swap_out()尝试交换出去。一旦某个页面成功交换到交换设备中就返回1。若对所
    // 有目录项的所有页表都已尝试失败，则显示"交换内存用完"的警告，并返回0。
    pg_table &= 0xfffff000;					/* 页表指针(地址)(页对齐) */
    while (counter-- > 0) {
        page_entry++;
        // 如果已经尝试处理完当前页表所有项还没有能够成功地交换出一个页面，即此时页表项索引
        // 大于等于1024，则如同前面第135-143行执行相同的处理来选出一个二级页表存在的页目
        // 录项，并取得相应二级页表指针。
        if (page_entry >= 1024) {
            page_entry = 0;
        repeat:
            dir_entry++;
            if (dir_entry >= 1024)
                dir_entry = FIRST_VM_PAGE >> 10;
            pg_table = pg_dir[dir_entry];	/* 页目录项内容 */
            if (!(pg_table & 1))
                if ((counter -= 1024) > 0)
                    goto repeat;
                else
                    break;
            pg_table &= 0xfffff000;			/* 页表指针 */
        }
        if (try_to_swap_out(page_entry + (unsigned long *) pg_table))
            return 1;
        }
    printk("Out of swap-memory\n\r");
    return 0;
}

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
/*
 * 获取首个(实际上是最后1个:-)空闲页面，并标志为已使用。如果没有空闲页面，就返回0。
 */
// 在主内存区中申请1页空闲物理页面
// 如果已经没有可用物理页面，则调用执行交换处理。然后再次申请页面。
// 输入:%1(ax=0) - 0;
//		%2(LOW_MEM)内存字节位图管理的起始位置;
// 		%3(cx=PAGING_PAGES);
// 		%4(edi=mem_map+PAGING_PAGES-1)
// 输出:返回%0(ax=物理页面起始地址)。函数返回新页面的物理地址。
// 上面%4寄存器实际指向mem_map[]内存字节位图的最后一个字节。本函数从位图末端开始
// 向前扫描所有页面标志(页面总数为PAGING_AGES)，若有页面空闲(内存位图字节为0)则
// 返回页面地址。注意!本函数只是指出在主内存区的一页空闲物理页面，但并没有映射到某
// 个进程的地址空间中去。后面的put_page()函数即用于把指定页面映射到某个进程的地址
// 空间中。当然对于内核使用本函数并不需要再使用put_page()进行映射，因为内核代码和
// 数据空间(16MB)已经对等地映射到物理地址空间。
unsigned long get_free_page(void)
{
register unsigned long __res;

// 首先在内存映射字节位图中查找址为0的字节项，然后把对应物理内存页面清零。如果得到
// 的页面地址大于实际物理内存容量则重新寻找。如果没有找到空闲页面则去调用执行交换处
// 理，并重新查找。最后返回空闲物理页面地址。
repeat:
    __asm__("std ; repne ; scasb\n\t"
        "jne 1f\n\t"
        "movb $1,1(%%edi)\n\t"
        "sall $12,%%ecx\n\t"
        "addl %2,%%ecx\n\t"
        "movl %%ecx,%%edx\n\t"
        "movl $1024,%%ecx\n\t"
        "leal 4092(%%edx),%%edi\n\t"
        "rep ; stosl\n\t"
        "movl %%edx,%%eax\n"
        "1:"
        :"=a" (__res)
        :"0" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),
        "D" (mem_map + PAGING_PAGES - 1)
        );
    if (__res >= HIGH_MEMORY) {	/* 页面地址大于实际内存容量则重新寻找 */
        goto repeat;
    }
    if (!__res && swap_out()) {	/* 若没有得到空闲页面则执行交换处理,并重新查找*/
        goto repeat;
    }
    return __res;				/* 返回空闲物理页面地址 */
}

/**
 * 内存交换初始化
 * @param[in]	void
 * @retval		void
 */
void init_swapping(void)
{
    extern int *blk_size[];
    int swap_size, i, j;

    if (!SWAP_DEV) {
        return;
    }
    if (!blk_size[MAJOR(SWAP_DEV)]) {
        printk("Unable to get size of swap device\n\r");
        return;
    }
    swap_size = blk_size[MAJOR(SWAP_DEV)][MINOR(SWAP_DEV)];
    if (!swap_size)
        return;
    if (swap_size < 100) {
        printk("Swap device too small (%d blocks)\n\r", swap_size);
        return;
    }
    swap_size >>= 2;
    if (swap_size > SWAP_BITS)
        swap_size = SWAP_BITS;
    swap_bitmap = (char *) get_free_page();
    if (!swap_bitmap) {
        printk("Unable to start swapping: out of memory :-)\n\r");
        return;
    }
    read_swap_page(0, swap_bitmap);
    if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10)) {
        printk("Unable to find swap-space signature\n\r");
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    memset(swap_bitmap + 4086, 0, 10);
    for (i = 0 ; i < SWAP_BITS ; i++) {
        if (i == 1)
            i = swap_size;
        if (bit(swap_bitmap, i)) {
            printk("Bad swap-space bit-map\n\r");
            free_page((long) swap_bitmap);
            swap_bitmap = NULL;
            return;
        }
    }
    j = 0;
    for (i = 1 ; i < swap_size ; i++)
        if (bit(swap_bitmap, i))
            j++;
    if (!j) {
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    printk("Swap device ok: %d pages (%d bytes) swap-space\n\r",j,j*4096);
}
