/*
 * This file contains the procedures for the handling of select
 *
 * Created for Linux based loosely upon Mathius Lattner's minix
 * patches by Peter MacDonald. Heavily edited by Linus.
 */
/*
 * 本文件含有处理select()系统调用的过程。
 *
 * 这是Peter MacDonald基于Mathius Lattner提供给MINIX系统的补丁程序修改而成。
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>

#include <asm/segment.h>
#include <asm/system.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <const.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>

/*
 * Ok, Peter made a complicated, but straightforward multiple_wait() function.
 * I have rewritten this, taking some shortcuts: This code may not be easy to
 * follow, but it should be free of race-conditions, and it's practical. If you
 * understand what I'm doing here, then you understand how the linux sleep/wakeup
 * mechanism works.
 *
 * Two very simple procedures, add_wait() and free_wait() make all the work. We
 * have to have interrupts disabled throughout the select, but that's not really
 * such a loss: sleeping automatically frees interrupts when we aren't in this
 * task.
 */
/*
 * OK，Peter编制了复杂但很直观的多个_wait()函数。我对这些函数进行了改写，以使之更简洁：这些代码
 * 可能不容易看懂，但是其中应该不会存在竞争条件问题，并且很实际。如果你能理解这里编制的代码，那么
 * 就说明你已经理解Linux中睡眠/唤醒的工作机制。
 *
 * 两个很简单的过程，add_wait()和free_wait()执行了主要操作。在整个select处理过程中我们不得不禁
 * 止中断。但是这样做并不会带来太多的损失：因为当我们不在执行本任务时睡眠状态会自动释放中断(即其他
 * 任务会使用自己EFLAGS中的中断标志)。
 */

typedef struct {
	struct task_struct * old_task;
	struct task_struct ** wait_address;
} wait_entry;

typedef struct {
	int nr;
	wait_entry entry[NR_OPEN * 3];
} select_table;

/**
 * 把未准备好描述符的等待队列指针加入等待表wait_table中
 * @param[in]	*wait_address	与描述符相关的等待队列头指针。如tty读缓冲队列secondary的
 *								等待队列头指针是proc_list
 * @param[in]	p				do_select()中定义的等待表结构指针
 * @retval		void
 */
static void add_wait(struct task_struct ** wait_address, select_table * p)
{
	int i;

	if (!wait_address) {
		return;
	}
	for (i = 0; i < p->nr; i++) {
		if (p->entry[i].wait_address == wait_address) {
			return;
		}
	}
	p->entry[p->nr].wait_address = wait_address;
	p->entry[p->nr].old_task = * wait_address;
	*wait_address = current;
	p->nr ++;
}


/**
 * 清空等待表
 * 本函数在do_select()函数中睡眠后被唤醒返回时被调用，用于唤醒等待表中处于各个等待队列上的其他
 * 任务，它与kernel/sched.c中sleep_on()函数的后半部分代码几乎完全相同，请参考对sleep_on()函数的
 * 说明。
 * @param[in]	p		等待表结构指针
 * @return		void
 */
static void free_wait(select_table * p)
{
	int i;
	struct task_struct ** tpp;

	for (i = 0; i < p->nr ; i++) {
		tpp = p->entry[i].wait_address;
		while (*tpp && *tpp != current) {
			(*tpp)->state = 0;
			current->state = TASK_UNINTERRUPTIBLE;
			schedule();
		}
		if (!*tpp) {
			printk("free_wait: NULL");
		}
		if ((*tpp = p->entry[i].old_task)) {
			(**tpp).state = 0;
		}
	}
	p->nr = 0;
}


/**
 * 根据文件i节点判断文件是不是字符终端设备文件
 * @param[in]	inode	i节点
 * @retval		若是则返回其tty结构指针，否则返回NULL
 */
static struct tty_struct * get_tty(struct m_inode * inode)
{
	int major, minor;

	if (!S_ISCHR(inode->i_mode)) {
		return NULL;
	}
	if ((major = MAJOR(inode->i_zone[0])) != 5 && major != 4) {
		return NULL;
	}
	if (major == 5) {
		minor = current->tty;
	} else {
		minor = MINOR(inode->i_zone[0]);
	}
	if (minor < 0) {
		return NULL;
	}
	return TTY_TABLE(minor);
}

/*
 * The check_XX functions check out a file. We know it's either
 * a pipe, a character device or a fifo (fifo's not implemented)
 */
/*
 * check_XX函数用于检查一个文件。我们知道该文件要么是管道文件、要么是字符设备文件，或者要么是
 * 一个FIFO（FIFO）还未实现。
 */

/**
 * 检查读文件操作是否准备好，即终端读缓冲队列secondary是否有字符可读，或者管道文件是否不空。
 * @param[in]	wait	等待表指针
 * @param[in]	inode	文件i节点指针
 * @retval		若描述符可进行读操作则返回1，否则返回0
 */
static int check_in(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

	if ((tty = get_tty(inode))) {
		if (!EMPTY(tty->secondary)) {
			return 1;
		} else {
			add_wait(&tty->secondary->proc_list, wait);
		}
	} else if (inode->i_pipe) {
		if (!PIPE_EMPTY(*inode)) {
			return 1;
		} else {
			add_wait(&inode->i_wait, wait);
		}
	}
	return 0;
}

/**
 * 检查文件写操作是否准备好，即终端写缓冲队列write_q中是否还有空闲位置可写，或者此时管道文件
 * 是否不满。
 * @param[in]	wait	等待表指针
 * @param[in]	inode	文件i节点指针
 * @retval		若描述符可进行写操作则返回1，否则返回0
 */
static int check_out(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

	if ((tty = get_tty(inode))) {
		if (!FULL(tty->write_q)) {
			return 1;
		} else {
			add_wait(&tty->write_q->proc_list, wait);
		}
	} else if (inode->i_pipe) {
		if (!PIPE_FULL(*inode)) {
			return 1;
		} else {
			add_wait(&inode->i_wait, wait);
		}
	}
	return 0;
}


/**
 * 检查文件是否处于异常状态。对于终端设备文件，目前内核总是返回0。对于管道文件，如果此时两个管
 * 道描述符中有一个或都已被关闭，则返回1，否则就把当前任务添加到管道i节点的等待队列上并返回0。
 * 返回0。
 * @param[in]	wait	等待表指针
 * @param[in]	inode	文件i节点指针
 * 若出现异常条件则返回1，否则返回0
 */
static int check_ex(select_table * wait, struct m_inode * inode)
{
	struct tty_struct * tty;

	if ((tty = get_tty(inode))) {
		if (!FULL(tty->write_q)) {
			return 0;
		} else {
			return 0;
		}
	} else if (inode->i_pipe) {
		if (inode->i_count < 2) {
			return 1;
		} else {
			add_wait(&inode->i_wait, wait);
		}
	}
	return 0;
}


/**
 * do_select()是内核执行select()系统调用的实际处理函数。该函数首先检查描述符集中各个描述符的有
 * 效性，然后分别调用相关描述符集描述符检查函数check_XX()对每个描述符进行检查，同时统计描述符
 * 集中当前已经准备好的描述符个数。若有任何一个描述符已经准备好，本函数就会立刻返回，否则进程
 * 就会在本函数中进入睡眠状态，并在过了超时时间或者由于某个描述符所在等待队列上的进程被唤醒而
 * 使本进程继续运行。
 */
int do_select(fd_set in, fd_set out, fd_set ex,
	fd_set *inp, fd_set *outp, fd_set *exp)
{
	int count;
	select_table wait_table;
	int i;
	fd_set mask;

	mask = in | out | ex;
	for (i = 0 ; i < NR_OPEN ; i++,mask >>= 1) {
		if (!(mask & 1)) {
			continue;
		}
		if (!current->filp[i]) {
			return -EBADF;
		}
		if (!current->filp[i]->f_inode) {
			return -EBADF;
		}
		if (current->filp[i]->f_inode->i_pipe) {
			continue;
		}
		if (S_ISCHR(current->filp[i]->f_inode->i_mode)) {
			continue;
		}
		if (S_ISFIFO(current->filp[i]->f_inode->i_mode)) {
			continue;
		}
		return -EBADF;
	}
repeat:
	wait_table.nr = 0;
	*inp = *outp = *exp = 0;
	count = 0;
	mask = 1;
	for (i = 0 ; i < NR_OPEN ; i++, mask += mask) {
		if (mask & in)
			if (check_in(&wait_table,current->filp[i]->f_inode)) {
				*inp |= mask;
				count++;
			}
		if (mask & out)
			if (check_out(&wait_table,current->filp[i]->f_inode)) {
				*outp |= mask;
				count++;
			}
		if (mask & ex)
			if (check_ex(&wait_table,current->filp[i]->f_inode)) {
				*exp |= mask;
				count++;
			}
	}
	if (!(current->signal & ~current->blocked) &&
	    (wait_table.nr || current->timeout) && !count) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		free_wait(&wait_table);
		goto repeat;
	}
	free_wait(&wait_table);
	return count;
}

/*
 * Note that we cannot return -ERESTARTSYS, as we change our input
 * parameters. Sad, but there you are. We could do some tweaking in
 * the library function ...
 */
/*
 * 注意我们不能返回-ERESTARTSYS，因为我们会在select运行过程中改变输入参数值(*timeout)。很不幸，
 * 但你也只能接受这个事实。不过我们可以在库函数中做些处理...
 */
/**
 * select 系统调用
 * 该函数中的代码主要负责进行select功能操作前后的参数复制和转换工作。select主要的工作由
 * do_select()函数来完成。sys_select()会首先根据参数传递来的缓冲区指针从用户数据空间把select()
 * 函数调用的参数分解复制到内核空间，然后设置需要等待的超时时间值timeout，接着调用do_select()执
 * 行select功能，返回后就把处理结果再复制回用户空间中。
 * @param[in]	buffer		指向用户数据区中select()函数的第1个参数处
 * @retval		如果返回值小于0，表示执行时出现错误；
 * 				如果返回值等于0，表示在规定等待时间内没有描述符准备好操作；
 *				如果返回值大于0,则表示已准备好的描述符数量。
 */
int sys_select( unsigned long *buffer )
{
/* Perform the select(nd, in, out, ex, tv) system call. */
	int i;
	fd_set res_in, in = 0, *inp;
	fd_set res_out, out = 0, *outp;
	fd_set res_ex, ex = 0, *exp;
	fd_set mask;
	struct timeval *tvp;
	unsigned long timeout;

	mask = ~((~0) << get_fs_long(buffer++));
	inp = (fd_set *) get_fs_long(buffer++);
	outp = (fd_set *) get_fs_long(buffer++);
	exp = (fd_set *) get_fs_long(buffer++);
	tvp = (struct timeval *) get_fs_long(buffer);

	if (inp) {
		in = mask & get_fs_long(inp);
	}
	if (outp) {
		out = mask & get_fs_long(outp);
	}
	if (exp) {
		ex = mask & get_fs_long(exp);
	}
	timeout = 0xffffffff;
	if (tvp) {
		timeout = get_fs_long((unsigned long *)&tvp->tv_usec) / (1000000 / HZ);
		timeout += get_fs_long((unsigned long *)&tvp->tv_sec) * HZ;
		timeout += jiffies;
	}
	current->timeout = timeout;
	cli();
	i = do_select(in, out, ex, &res_in, &res_out, &res_ex);
	if (current->timeout > jiffies) {
		timeout = current->timeout - jiffies;
	} else {
		timeout = 0;
	}
	sti();
	current->timeout = 0;
	if (i < 0)
		return i;
	if (inp) {
		verify_area(inp, 4);
		put_fs_long(res_in, inp);
	}
	if (outp) {
		verify_area(outp, 4);
		put_fs_long(res_out, outp);
	}
	if (exp) {
		verify_area(exp, 4);
		put_fs_long(res_ex, exp);
	}
	if (tvp) {
		verify_area(tvp, sizeof(*tvp));
		put_fs_long(timeout/HZ, (unsigned long *) &tvp->tv_sec);
		timeout %= HZ;
		timeout *= (1000000/HZ);
		put_fs_long(timeout, (unsigned long *) &tvp->tv_usec);
	}
	if (!i && (current->signal & ~current->blocked)) {
		return -EINTR;
	}
	return i;
}
