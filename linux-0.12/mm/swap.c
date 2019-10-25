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

/* 1页(4096B)共有32768个位。最多可管理32768个页面，对应128MB内存容量 */
#define SWAP_BITS (4096 << 3)

/**
 * 通过给定不同的"op"，可定义对指定比特位进行测试，设置或清除三种操作。该宏把给定地
 * 址addr处第nr个比特位的值放入进位标志，设置或复位该比特位并返回进位标志值(即原位值)。
 * @param[in]   addr    指定线性地址
 * @param[in]   nr      指定地址处开始的比特位偏移位
 * @return      原比特位值  
 */
#define bitop(name, op) 								\
static inline int name(char * addr, unsigned int nr) 	\
{ 														\
    int __res; 											\
    __asm__ __volatile__("bt" op " %1, %2; adcl $0, %0" \
        :"=g" (__res) 									\
        :"r" (nr),"m" (*(addr)),"0" (0)); 				\
    return __res; 										\
}

/* 这里根据不同的op字符定义3个内嵌函数：
 * 当op=""时，就是指令bt - (Bit Test)测试并返回原值；
 * 当op="s"时，就是指令bts - (Bit Test and Set)设置比特位值并返回原值；
 * 当op="r"时，就是指令btr - (Bit Test and Reset)复位比特位值并返回原值。
 */
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
/* = 64MB/4KB 第1个虚拟内存页面，即从任务0末端(64MB)处开始的虚拟内存页面 */
#define FIRST_VM_PAGE (TASK_SIZE >> 12)

/* = 4GB/4KB 4G内存空间对应的页数 */
#define LAST_VM_PAGE (1024 * 1024)

#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE)

/**
 * 申请1页交换页面
 * 扫描整个交换映射位图(除对应位图本身的位0以外)，返回值为1的第一个比特位号
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
            return nr;
        }
    }
    return 0;
}

/**
 * 释放交换设备中指定的交换页面
 * 在交换位图中设置指定页面号对应的位(置1)。若原来该位就等于1，则表示交换设备中原来该页面就没
 * 有被占用，或者位图出错。于是显示出错信息并返回。
 * @param[in]	swap_nr	交换页面号
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

    if (!swap_bitmap) {
        printk("Trying to swap in without swap bit-map");
        return;
    }
    if (1 & *table_ptr) { /* 指定页表项对应的页面已存在于内存中 */
        printk("trying to swap in present page\n\r");
        return;
    }
    /* 在交换设备中的页面对应的页表项中存放的是交换页面号swap_nr<<1，最低位P=0 */
    swap_nr = *table_ptr >> 1;
    if (!swap_nr) { /* 交换页面号为0 */
        printk("No swap page in swap_in\n\r");
        return;
    }
    if (!(page = get_free_page())) {
        oom();
    }
    read_swap_page(swap_nr, (char *) page);
    if (setbit(swap_bitmap, swap_nr)) {
        printk("swapping in multiply from same page\n\r");
    }
    /* 让页表指向该物理页面，并设置标志位(Dirty,U/S,R/W,P)。*/
    *table_ptr = page | (PAGE_DIRTY | 7);
}

/**
 * 尝试把页面交换出去(仅在swap_out中被调用)
 * 1. 页面未被修改过，则不必换出，直接释放即可，因为对应页面还可以再直接从相应映像文件中读入
 * 2. 页面被修改过，则尝试换出。
 * @param[in]   table_ptr   页表项指针
 * @return      页面换或释放成功返回1，失败返回0
 */
/*static*/ int try_to_swap_out(unsigned long * table_ptr)
{
    unsigned long page;
    unsigned long swap_nr;

    page = *table_ptr;
    if (!(PAGE_PRESENT & page)) { /* 要换出的页面不存在 */
        return 0;
    }
    if (page - LOW_MEM > PAGING_MEMORY) { /* 指定物理内存地址高于内存高端或低于LOW_MEM */
        return 0;
    }
    if (PAGE_DIRTY & page) { /* 内存页面已被修改过 */
        page &= 0xfffff000;
        if (mem_map[MAP_NR(page)] != 1) {   /* 页面又是被共享的，不宜换出 */
            return 0;
        }
        if (!(swap_nr = get_swap_page())) {
            return 0;
        }
        /* 换出页面的页表项的内容为(swap_nr << 1)|(P = 0) */
        *table_ptr = swap_nr << 1;
        invalidate();
        write_swap_page(swap_nr, (char *) page);
        free_page(page);
        return 1;
    }
    /* 执行到这表明页面没有修改过，直接释放即可 */
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
/**
 * 把内存页面交换到交换设备中(仅在get_free_page被调用)
 * 从线性地址64MB对应的目录项(FIRST_VM_PAGE>>10)开始，搜索整个4GB线性空间，对有效页目录
 * 二级页表指定的物理内存页面执行交换到交换设备中去的尝试。
 * @return  成功返回1，失败返回0
 */
/*static*/ int swap_out(void)
{
    static int dir_entry = FIRST_VM_PAGE >> 10;	/* 即任务1的第1个目录项索引 */
    static int page_entry = -1;
    int counter = VM_PAGES;			/* 表示除去任务0以外的其他任务的所有页数目 */
    int pg_table;

    /* 首先搜索页目录表，查找第一个有效的页目录项pg_table */
    while (counter > 0) {
        pg_table = pg_dir[dir_entry];
        if (pg_table & 1) {
            break;
        }
        counter -= 1024;    /* 1个页表对应1024个页帧 */
        dir_entry++;
        if (dir_entry >= 1024) {
            /* 检索完整个页目录表，重新从头开始检索（执行不到） */
            dir_entry = FIRST_VM_PAGE >> 10;
        }
    }
    /* 对取到页目录项对应页表中的页表项开始逐一调用交换函数 */
    pg_table &= 0xfffff000;
    while (counter-- > 0) {
        page_entry++;
        if (page_entry >= 1024) {
            /* 页表项索引>=1024，则取下一个有效的页目录项 */
            page_entry = 0;
        repeat:
            dir_entry++;
            if (dir_entry >= 1024) {
                /* 检索完整个页目录表，重新从头开始检索（执行不到） */
                dir_entry = FIRST_VM_PAGE >> 10;
            }
            pg_table = pg_dir[dir_entry];
            if (!(pg_table & 1)) {
                if ((counter -= 1024) > 0) {
                    goto repeat;
                } else {
                    break;
                }
            }
            pg_table &= 0xfffff000;
        }
        if (try_to_swap_out(page_entry + (unsigned long *) pg_table)) {
            /* 成功换出一个页面即退出 */
            return 1;
        }
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

/**
 * 在主内存区中申请1页空闲物理页面
 * @return  空闲的页面地址
 */
unsigned long get_free_page(void)
{
	register unsigned long __res;

/* 在内存映射字节位图中从尾到头地查找值为0的字节项，然后把对应物理内存页面清零 */
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
    if (__res >= HIGH_MEMORY) {	/* 页面地址大于实际内存容量，重新寻找 */
        goto repeat;
    }
    if (!__res && swap_out()) {	/* 没有得到空闲页面则执行交换处理,并重新查找 */
        goto repeat;
    }
    return __res;
}

/**
 * 内存交换初始化
 * @note 在交换页面位图中，swap_bitmap[0]和swap_bitmap[swap_size ~ SWAP_BITS-1]不可用，
 *       swap_bitmap[1 ~ swap_size-1]可用
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
    if (!swap_size) {
        return;
    }
    if (swap_size < 100) {
        printk("Swap device too small (%d blocks)\n\r", swap_size);
        return;
    }
    swap_size >>= 2;  /* swap_size以1KB为单位，>>2则表示有多少个4KB */
    if (swap_size > SWAP_BITS) {
        swap_size = SWAP_BITS;
    }
    swap_bitmap = (char *) get_free_page();
    if (!swap_bitmap) {
        printk("Unable to start swapping: out of memory :-)\n\r");
        return;
    }
    read_swap_page(0, swap_bitmap);
    /* 设备交换分区的页面0的4086起的10个字节应该含有特征字符串“SWAP-SPACE” */
    if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10)) {
        printk("Unable to find swap-space signature\n\r");
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    memset(swap_bitmap + 4086, 0, 10);
    /* 检查不可用的比特位（[0]，[swap_size ~ SWAP_BITS-1]） */
    for (i = 0 ; i < SWAP_BITS ; i++) {
        if (i == 1) {
            i = swap_size;
        }
        if (bit(swap_bitmap, i)) {
            printk("Bad swap-space bit-map\n\r");
            free_page((long) swap_bitmap);
            swap_bitmap = NULL;
            return;
        }
    }
    j = 0;
    /* 统计空闲交换页面（1为空闲） */
    for (i = 1 ; i < swap_size ; i++) {
        if (bit(swap_bitmap, i)) {
            j++;
        }
    }
    if (!j) {
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    printk("Swap device ok: %d pages (%d bytes) swap-space\n\r", j, j*4096);
}
