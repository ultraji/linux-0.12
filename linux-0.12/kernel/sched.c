/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */

/*
 * 'sched.c'是主要的内核文件。其中包括有关高度的基本函数(sleep_on，wakeup，schedule等)以及一些
 * 简单的系统调用函数(比如getpid()，仅从当前任务中获取一个字段)。
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

/* 该宏取信号nr在信号位图中对应位的二进制数值。信号编号1-32 */
#define _S(nr) 		(1<<((nr)-1))

/* 除了SIGKILL和SIGSTOP信号以外其他信号都是可阻塞的 */
#define _BLOCKABLE 	(~(_S(SIGKILL) | _S(SIGSTOP)))

/* static */ void show_task(int nr, struct task_struct * p)
{
	int i, j = 4096 - sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, father=%d, child=%d, ", nr, p->pid,
		p->state, p->p_pptr->pid, p->p_cptr ? p->p_cptr->pid : -1);
	i = 0;
	while (i < j && !((char *)(p+1))[i]) {
		i++;
	}
	printk("%d/%d chars free in kstack\n\r",i,j);
	printk("   PC=%08X.", *(1019 + (unsigned long *) p));
	if (p->p_ysptr || p->p_osptr) {
		printk("   Younger sib=%d, older sib=%d\n\r", 
			p->p_ysptr ? p->p_ysptr->pid : -1,
			p->p_osptr ? p->p_osptr->pid : -1);
	} else {
		printk("\n\r");
	}
}

void show_state(void)
{
	int i;

	printk("\rTask-info:\n\r");
	for (i = 0; i < NR_TASKS; i++) {
		if (task[i]) {
			show_task(i, task[i]);
		}
	}
}

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

unsigned long volatile jiffies=0;
unsigned long startup_time=0;
int jiffies_offset = 0;		/* # clock ticks to add to get "true
				   time".  Should always be less than
				   1 second's worth.  For time fanatics
				   who like to syncronize their machines
				   to WWV :-) */

struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current) {
		return;
	}
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math = 1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */

/*
 * 'schedule()' 是一个调度函数。这是一块很好的代码，没有理由去修改它，因为它可以在所有的环境下工
 * 作(比如能够对IO-边界下得很好的响应等)。只有一件事值得留意，那就是这里的信号处进代码。
 * 
 * 注意!!任务0是个闲置('idle')任务，只有当没有其他任务可以运行时才调用它。它不能被杀死，也不睡眠。
 * 任务0中的状态信息'state'是从来不用的。
 * 
 */
void schedule(void)
{
	int i, next, c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->timeout && (*p)->timeout < jiffies) {
				(*p)->timeout = 0;
				if ((*p)->state == TASK_INTERRUPTIBLE)
					(*p)->state = TASK_RUNNING;
			}
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1<<(SIGALRM-1));
				(*p)->alarm = 0;
			}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE) {
				(*p)->state=TASK_RUNNING;
			}
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

/**
 * 转换当前任务的状态为可中断的等待状态
 * 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程调用一个信号捕
 * 获函数。只有当捕获了一个信号，并且信号捕获处理函数返回，pause()才会返回。此时pause()返回值应
 * 该是-1，并且errno被置为EINTR。这里还没有完全实现(直到0.95版)
 */
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

static inline void __sleep_on(struct task_struct **p, int state)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = state;
repeat:	schedule();
	if (*p && *p != current) {
		(**p).state = 0;
		current->state = TASK_UNINTERRUPTIBLE;
		goto repeat;
	}
	if (!*p)
		printk("Warning: *P = NULL\n\r");
	if (*p = tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	__sleep_on(p,TASK_INTERRUPTIBLE);
}

void sleep_on(struct task_struct **p)
{
	__sleep_on(p,TASK_UNINTERRUPTIBLE);
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		if ((**p).state == TASK_STOPPED)
			printk("wake_up: TASK_STOPPED");
		if ((**p).state == TASK_ZOMBIE)
			printk("wake_up: TASK_ZOMBIE");
		(**p).state=0;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

/**
 * 时钟中断C函数处理程序
 * 对于一个进程由于执行时间片用完时，则进行任务切换，并执行一个计时更新工作。在sys_call.s中
 * 的timer_interrupt被调用。
 * @param[in]	cpl		当前特权级，是时钟中断发生时正被执行的代码选择符中的特权级
 * @retval		void
 */
void do_timer(long cpl)
{
	static int blanked = 0;

	if (blankcount || !blankinterval) {
		if (blanked) {
			unblank_screen();
		}
		if (blankcount) {
			blankcount--;
		}
		blanked = 0;
	} else if (!blanked) {
		blank_screen();
		blanked = 1;
	}
	if (hd_timeout) {
		if (!--hd_timeout) {
			hd_times_out();
		}
	}
	if (beepcount) {
		if (!--beepcount) {
			sysbeepstop();
		}
	}
	if (cpl) {
		current->utime++;
	} else {
		current->stime++;
	}
	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0) {
		do_floppy_timer();
	}
	if ((--current->counter)>0) {
		return;
	}
	current->counter=0;
	if (!cpl) {
		return;
	}
	schedule();
}

/**
 * 设置报警定时时间值（秒）
 * @note 		alarm的单位是系统滴答（1滴答为10毫秒）,它是系统开机起到设置定时操作时系统滴答值
 * 				jiffies和转换成滴答单位的定时值之和，即'jiffies + HZ*定时秒值'
 * @param[in]	seconds		新的定时时间值(单位是秒)
 * @retval		若参数seconds大于0，则设置新定时值，并返回原定时时刻还剩余的间隔时间；否则返回0。
 */
int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old) {
		old = (old - jiffies) / HZ;
	}
	current->alarm = (seconds > 0) ? (jiffies + HZ * seconds) : 0;
	return (old);
}

/* 取进程号pid */
int sys_getpid(void)
{
	return current->pid;
}

/* 取父进程号ppid */
int sys_getppid(void)
{
	return current->p_pptr->pid;
}

/* 取用户id */
int sys_getuid(void)
{
	return current->uid;
}

/* 取有效用户id */
int sys_geteuid(void)
{
	return current->euid;
}

/* 取组号gid */
int sys_getgid(void)
{
	return current->gid;
}

/* 取有效的组号egid */
int sys_getegid(void)
{
	return current->egid;
}

/**
 * 改变对cpu的使用优先权
 * @param[in]	increment
 * @retval		0
 */
int sys_nice(long increment)
{
	if (current->priority-increment > 0) {
		current->priority -= increment;
	}
	return 0;
}

/* 内核调度程序的初始化子程序 */
void sched_init(void)
{
	int i;
	struct desc_struct * p;	/* 描述符表结构指针 */

	/* 这个判断语句并无必要 */
	if (sizeof(struct sigaction) != 16) {
		panic("Struct sigaction MUST be 16 bytes");
	}
	set_tss_desc(gdt+FIRST_TSS_ENTRY, &(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY, &(init_task.task.ldt));
	p = gdt + 2 + FIRST_TSS_ENTRY;
	for(i = 1; i < NR_TASKS; i++) {
		task[i] = NULL;
		p->a = p->b = 0;
		p++;
		p->a = p->b = 0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36,0x43);				/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);		/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}
