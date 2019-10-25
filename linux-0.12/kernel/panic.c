/*
 *  linux/kernel/panic.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This function is used through-out the kernel (includeinh mm and fs)
 * to indicate a major problem.
 */
#include <linux/kernel.h>
#include <linux/sched.h>

void sys_sync(void);	/* it's really int */
						/* 实际上该函数返回int的 */

/**
 * 显示内核中出现的重大错误信息，并运行文件系统同步函数,然后进入死循环
 * @note 如果当前进程是任务0的话，还说明是交换任务出错，并且还没有运行文件系统同步函数。
 */
volatile void panic(const char * s)
{
	printk("Kernel panic: %s\n\r",s);
	if (current == task[0]) {
		printk("In swapper task - not syncing\n\r");
	} else {
		sys_sync();
	}
	for(;;);
}
