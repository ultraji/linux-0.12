/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
/*
 * 'fork.c'中含有系统调用'fork'的辅助子程序，以及一些其他函数('verify_area')。一旦你了解了
 * fork，就会发现它非常简单的，但内存管理却有些难度。
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

/* 写页面验证 */
extern void write_verify(unsigned long address);

/* 最新进程号，其值会由get_empty_process()生成，会不断增加，无上限；系统同时容纳的最多任务
 数有上限（NR_TASKS = 64） */
static long last_pid = 0;	

/**
 * 进程空间区域的写前验证
 * 对于80386 CPU，在执行内核代码时用户空间中的R/W标志起不了作用，写时复制机制失效了。所以
 * 我们得手动做这个写前验证。
 * @param[in]		addr	需要写验证的逻辑地址起始位置
 * @param[in]		size	需要写验证的长度（单位为字节）
 * @retval			void
 */
void verify_area(void * addr, int size)
{
    unsigned long start;

    start = (unsigned long) addr;
    size += start & 0xfff;
    start &= 0xfffff000;
    start += get_base(current->ldt[2]);
    while (size > 0) {
        size -= 4096;
        write_verify(start);
        start += 4096;
    }
}

/**
 * 复制内存页表
 * 该函数为新任务在线性地址空间中设置代码段和数据段基址，限长，并复制页表。由于Linux系统采用写时复制
 * (copy on write)技术，因此这里仅为新进程设置自己的页目录表项和页表项，而没有实际为新进程分配物理内
 * 存页面。此时新进程与其父进程共享所有内存页面。
 * @param[in]		nr		新任务号
 * @param[in]		p		新任务的数据结构指针
 * @retval			成功返回0，失败返回出错号
*/
static int copy_mem(int nr, struct task_struct * p)
{
    unsigned long old_data_base, new_data_base, data_limit;
    unsigned long old_code_base, new_code_base, code_limit;

    code_limit = get_limit(0x0f);
    data_limit = get_limit(0x17);
    old_code_base = get_base(current->ldt[1]);
    old_data_base = get_base(current->ldt[2]);
    if (old_data_base != old_code_base) {
        panic("We don't support separate I&D");
    }
    if (data_limit < code_limit) {
        panic("Bad data_limit");
    }
    new_data_base = new_code_base = nr * TASK_SIZE;
    p->start_code = new_code_base;
    set_base(p->ldt[1], new_code_base);
    set_base(p->ldt[2], new_data_base);
    if (copy_page_tables(old_data_base, new_data_base, data_limit)) {
        free_page_tables(new_data_base, data_limit);
        return -ENOMEM;
    }
    return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
/*
 * OK，下面是主要的fork子程序。它复制系统进程信息(task[n])，并且设置必要的寄存器。它还整个地复制数据段(也是
 * 代码段)。
 */

/**
 * 复制进程
 * sys_call.s中sys_fork会首先调用find_empty_process会更新last_pid，然后压入一些参数，再调用copy_process。
 * @param[in]	nr,ebp,edi,esi,gs               find_empty_process分配的任务数组项号nr，调用copy_process之前
 *                                              入栈的gs，esi，edi，ebp
 * @param[in]   none                            sys_fork函数入栈的返回地址
 * @param[in]	ebx,ecx,edx,orig_eax,fs,es,ds   system_call时入栈的段寄存器ds，es，fs和edx，ecx，ebx
 * @param[in]	eip,cs,eflags,esp,ss            CPU执行中断指令压入的用户栈地址ss和esp，标志eflags和返回地址cs和eip
 * @return      成功返回最新的PID，失败返回错误号
 */
int copy_process(int nr, long ebp, long edi, long esi, long gs, long none,
        long ebx, long ecx, long edx, long orig_eax, 
        long fs, long es, long ds,
        long eip, long cs, long eflags, long esp, long ss)
{
    struct task_struct *p;
    int i;
    struct file *f;
    /* 为新任务数据结构分配内存 */
    p = (struct task_struct *) get_free_page();
    if (!p) {
        return -EAGAIN;
    }
    task[nr] = p;
    *p = *current;	    /* NOTE! this doesn't copy the supervisor stack */

    /* 对复制来的进程结构内容进行一些修改。先将新进程的状态置为不可中断等待状态，以防止内核调度其执行 */
    p->state = TASK_UNINTERRUPTIBLE;
    p->pid = last_pid;
    p->counter = p->priority;
    p->signal = 0;
    p->alarm = 0;
    p->leader = 0;		/* process leadership doesn't inherit */
    p->utime = p->stime = 0;
    p->cutime = p->cstime = 0;
    p->start_time = jiffies;

    /* 修改任务状态段TSS内容 */
    p->tss.back_link = 0;
    p->tss.esp0 = PAGE_SIZE + (long) p; /* (PAGE_SIZE + (long) p)让esp0正好指向该页顶端 */
    p->tss.ss0 = 0x10;
    p->tss.eip = eip;
    p->tss.eflags = eflags;
    p->tss.eax = 0;
    p->tss.ecx = ecx;
    p->tss.edx = edx;
    p->tss.ebx = ebx;
    p->tss.esp = esp;
    p->tss.ebp = ebp;
    p->tss.esi = esi;
    p->tss.edi = edi;
    p->tss.es = es & 0xffff;
    p->tss.cs = cs & 0xffff;
    p->tss.ss = ss & 0xffff;
    p->tss.ds = ds & 0xffff;
    p->tss.fs = fs & 0xffff;
    p->tss.gs = gs & 0xffff;
    p->tss.ldt = _LDT(nr);
    p->tss.trace_bitmap = 0x80000000;
    /* 当前任务使用了协处理器，就保存其上下文 */
    if (last_task_used_math == current) {
        __asm__("clts ; fnsave %0 ; frstor %0"::"m" (p->tss.i387));
    }
    /* 复制父进程的内存页表，没有分配物理内存，共享父进程内存 */
    if (copy_mem(nr,p)) {
        task[nr] = NULL;
        free_page((long) p);
        return -EAGAIN;
    }
    /* 修改打开的文件，当前工作目录，根目录，执行文件，被加载库文件的使用数 */
    for (i = 0; i < NR_OPEN; i++) {
        if ((f = p->filp[i])) {
            f->f_count ++;
        }
    }
    if (current->pwd) {
        current->pwd->i_count++;
    }
    if (current->root) {
        current->root->i_count++;
    }
    if (current->executable) {
        current->executable->i_count++;
    }
    if (current->library) {
        current->library->i_count++;
    }

    /* 在GDT表中设置任务状态段描述符TSS和局部表描述符LDT */
    set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY, &(p->tss));
    set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY, &(p->ldt));

    /* 设置子进程的进程指针 */
    p->p_pptr = current;
    p->p_cptr = 0;
    p->p_ysptr = 0;
    p->p_osptr = current->p_cptr;
    if (p->p_osptr) {
        p->p_osptr->p_ysptr = p;
    }
    current->p_cptr = p;

    p->state = TASK_RUNNING;	/* do this last, just in case */

    return last_pid;
}

/**
 * 取得不重复的进程号last_pid
 * 由sys_fork调用为新进程取得不重复的进程号last_pid，并返回第一个空闲的任务结构数组索引号
 * @param[in]   void
 * @retval      成功返回在任务数组中的任务号(数组项)，失败返回错误号
 */
int find_empty_process(void)
{
    int i;

    repeat:
        if ((++last_pid) < 0) {
            last_pid = 1;
        }
        for(i = 0; i < NR_TASKS; i++) {
            if (task[i] && ((task[i]->pid == last_pid) ||
                        (task[i]->pgrp == last_pid))) {
                goto repeat;
            }
        }
    for(i = 1; i < NR_TASKS; i++) {
        if (!task[i]) {
            return i;
        }
    }
    return -EAGAIN;
}
