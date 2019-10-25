#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;			/* 信号原子类型操作 */
typedef unsigned int sigset_t;		/* 32 bits */

#define _NSIG             32		/* 信号种类 */
#define NSIG		_NSIG

#define SIGHUP		 1				/* Hang Up	-- 挂起控制终端或进程 */
#define SIGINT		 2				/* Interrupt -- 来自键盘的中断 */
#define SIGQUIT		 3				/* Quit		-- 来自键盘的退出 */
#define SIGILL		 4				/* Illeagle	-- 非法指令 */
#define SIGTRAP		 5				/* Trap 	-- 跟踪断点 */
#define SIGABRT		 6				/* Abort	-- 异常结束 */
#define SIGIOT		 6				/* IO Trap	-- 同上 */
#define SIGUNUSED	 7				/* Unused	-- 没有使用 */
#define SIGFPE		 8				/* FPE		-- 协处理器出错 */
#define SIGKILL		 9				/* Kill		-- 强迫进程终止 */
#define SIGUSR1		10				/* User1	-- 用户信号 1，进程可使用 */
#define SIGSEGV		11				/* Segment Violation -- 无效内存引用 */
#define SIGUSR2		12				/* User2    -- 用户信号 2，进程可使用 */
#define SIGPIPE		13				/* Pipe		-- 管道写出错，无读者 */
#define SIGALRM		14				/* Alarm	-- 实时定时器报警 */
#define SIGTERM		15				/* Terminate -- 进程终止 */
#define SIGSTKFLT	16				/* Stack Fault -- 栈出错（协处理器） */
#define SIGCHLD		17				/* Child	-- 子进程停止或被终止 */
#define SIGCONT		18				/* Continue	-- 恢复进程继续执行 */
#define SIGSTOP		19				/* Stop		-- 停止进程的执行 */
#define SIGTSTP		20				/* TTY Stop	-- tty 发出停止进程，可忽略 */
#define SIGTTIN		21				/* TTY In	-- 后台进程请求输入 */
#define SIGTTOU		22				/* TTY Out	-- 后台进程请求输出 */

/* Ok, I haven't implemented sigactions, but trying to keep headers POSIX */
/* 上面原注释已经过时，因为在0.12内核中已经实现了sigaction()。下面是sigaction结构
sa_flags标志字段可取的符号常数值 */
#define SA_NOCLDSTOP	1			/* 当子进程处于停止状态，就不对SIGCHLD处理 */
#define SA_INTERRUPT	0x20000000	/* 系统调用被信号中断后不重新启动系统调用 */
#define SA_NOMASK		0x40000000	/* 不阻止在指定的信号处理程序中再收到该信号 */
#define SA_ONESHOT		0x80000000 /* 信号句柄一旦被调用过就恢复到默认处理句柄 */

#define SIG_BLOCK          0	/* for blocking signals */ /* 在阻塞信号集中加上给定信号 */
#define SIG_UNBLOCK        1	/* for unblocking signals */ /* 从阻塞信号集中删除指定信号 */
#define SIG_SETMASK        2	/* for setting the signal mask */ /* 设置阻塞信号集 */

#define SIG_DFL		((void (*)(int))0)	/* default signal handling */	/* 默认信号处理程序（信号句柄） */
#define SIG_IGN		((void (*)(int))1)	/* ignore signal */	/* 忽略信号的处理程序 */
#define SIG_ERR		((void (*)(int))-1)	/* error return from signal */	/* 信号处理返回错误 */

#ifdef notdef
#define sigemptyset(mask) ((*(mask) = 0), 1)		/* 将mask清零 */
#define sigfillset(mask) ((*(mask) = ~0), 1)		/* 将mask所有比特位置位 */
#endif

struct sigaction {
	void (*sa_handler)(int);	/* 对应某信号指定要采取的行动 */
	sigset_t sa_mask;			/* 对信号的屏蔽码，在信号程序执行时将阻塞对这些信号的处理 */
	int sa_flags;				/* 改变信号处理过程的信号集 */
	void (*sa_restorer)(void);/* 恢复函数指针，由函数库Libc提供，用于清理用户态堆栈 */
};

void (*signal(int _sig, void (*_func)(int)))(int);
int raise(int sig);
int kill(pid_t pid, int sig);
int sigaddset(sigset_t *mask, int signo);
int sigdelset(sigset_t *mask, int signo);
int sigemptyset(sigset_t *mask);
int sigfillset(sigset_t *mask);
int sigismember(sigset_t *mask, int signo); /* 1 - is, 0 - not, -1 error */
int sigpending(sigset_t *set);
int sigprocmask(int how, sigset_t *set, sigset_t *oldset);
int sigsuspend(sigset_t *sigmask);
int sigaction(int sig, struct sigaction *act, struct sigaction *oldact);

#endif /* _SIGNAL_H */
