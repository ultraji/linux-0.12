#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096	/* 定义页面大小(字节数) */

#include <linux/kernel.h>
#include <signal.h>

extern int SWAP_DEV;

#define read_swap_page(nr,buffer) ll_rw_page(READ,SWAP_DEV,(nr),(buffer));
#define write_swap_page(nr,buffer) ll_rw_page(WRITE,SWAP_DEV,(nr),(buffer));

extern unsigned long get_free_page(void);
extern unsigned long put_dirty_page(unsigned long page,unsigned long address);
extern void free_page(unsigned long addr);
extern void init_swapping(void);
void swap_free(int page_nr);
void swap_in(unsigned long *table_ptr);

static inline void oom(void)
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

/* 刷新页变换高速缓冲（TLB）宏函数 */ 	
/* 为了提高地址转换的效率，CPU将最近使用的页表数据存放在芯片中高速缓冲中。在修改过页表信息之后，就
 需要刷新该缓冲区。这里使用重新加载页目录基址寄存器CR3的方法来进行刷新。下面eax=0是页目录的基址。*/
#define invalidate() __asm__("movl %%eax,%%cr3"::"a" (0))

/* these are not to be changed without changing head.s etc */
#define LOW_MEM 0x100000				/* 物理内存地址低端1MB */
extern unsigned long HIGH_MEMORY;		/* 物理内存地址高端 */
#define PAGING_MEMORY (15*1024*1024) /* 可分页的物理内存大小 */
#define PAGING_PAGES (PAGING_MEMORY>>12)	/* 可分页的物理内存的页面数 */
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)	/* 将物理内存地址映射成物理内存页面号 */
#define USED 100	/* 物理内存被占用 */

extern unsigned char mem_map [ PAGING_PAGES ];

#define PAGE_DIRTY		0x40		/* 脏位 */
#define PAGE_ACCESSED	0x20		/* 已访问位 */
#define PAGE_USER		0x04		/* 用户/超级用户位 */
#define PAGE_RW			0x02		/* 页面读写位 */
#define PAGE_PRESENT	0x01		/* 页面存在位 */

#endif
