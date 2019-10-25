#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* open/fcntl - NOCTTY, NDELAY isn't implemented yet */
/* open/fcntl - NOCTTY和NDELAY现在还没有实现 */
#define O_ACCMODE	00003	/* 文件访问模式屏蔽码 */
/* 文件访问模式：打开文件open()和文件控制函数fcntl()使用，同时只能使用三者之一 */
#define O_RDONLY	   00	/* 以只读方式打开文件 */
#define O_WRONLY	   01	/* 以只写方式打开文件 */
#define O_RDWR		   02	/* 以读写方式打开文件 */
/* 文件创建和操作：用于open()，可与上面访问模式用'位或'的方式一起使用 */
							/* fcntl()不用 */
#define O_CREAT		00100	/* not fcntl */	/* 如果文件不存在就创建 */
#define O_EXCL		00200	/* not fcntl */	/* 独占使用文件标志 */
#define O_NOCTTY	00400	/* not fcntl */	/* 不分配控制终端 */
#define O_TRUNC		01000	/* not fcntl */	/* 若文件已存在且是写操作，则长度截为0 */
#define O_APPEND	02000					/* 以追加方式打开,文件指针置为文件尾 */
#define O_NONBLOCK	04000	/* not fcntl */	/* 非阻塞方式打开和操作文件 */
#define O_NDELAY	O_NONBLOCK				/* 阻塞方式打开和操作文件 */

/* Defines for fcntl-commands. Note that currently
 * locking isn't supported, and other things aren't really
 * tested.
 */
/*
 * 下面定义了fcntl的命令。注意目录锁定命令还没有支持，而其他命令实际上还没测试过。
 */
/* 文件句柄(描述符)操作函数fcntl()的命令(cmd) */
#define F_DUPFD		0	/* dup */				/* 拷贝文件句柄为最小数值的句柄 */
#define F_GETFD		1	/* get f_flags */		/* 取句柄标志。仅1个标志FD_CLOEXEC */
#define F_SETFD		2	/* set f_flags */		/* 设置句柄标志 */
#define F_GETFL		3	/* more flags (cloexec) */  /* 取文件状态标志和访问模式 */
#define F_SETFL		4							/* 设置文件状态标志和访问模式 */
/* 下面3个是文件锁定命令。fnctl()的第三个参数lock是指向flock结构的指针(3个都未实现) */
#define F_GETLK		5	/* not implemented */	/* 返回阻止锁定的flock结构 */
#define F_SETLK		6	/* 设置(F_RDLCK或F_WRLCK)或清除(F_UNLCK)锁定 */
#define F_SETLKW	7	/* 等待设置或清除锁定 */

/* for F_[GET|SET]FL */
/* 用于F_GETFL或F_SETFL */
/* 在执行exec()簇函数时需要关闭的文件句柄（执行时关闭 - Close On EXECution）*/
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */
						/* 实际上只要低位为1即可 */

/* Ok, these are locking features, and aren't implemented at any
 * level. POSIX wants them.
 */
/*
 * OK，以下是锁定类型，任何函数中都还没有实现。POSIX标准要求这些类型。
 */
#define F_RDLCK		0	/* 共享或读文件锁定 */
#define F_WRLCK		1	/* 独占或写文件锁定 */
#define F_UNLCK		2	/* 文件解锁 */

/* Once again - not implemented, but ... */
/* 同样 - 也还没有实现，但是... */

/* 文件锁定操作数据结构 */
struct flock {
	short l_type;	/* 锁定类型(F_RDLCK，F_WRLCK，F_UNLCK) */
	short l_whence;	/* 开始偏移(SEEK_SET，SEEK_CUR或SEEK_END) */
	off_t l_start;	/* 阻塞锁定的开始处。相对偏移(字节数) */
	off_t l_len;	/* 阻塞锁定的大小。如果是则为到文件末尾 */
	pid_t l_pid;	/* 加锁的进程id */
};

/**** 以下是使用上述标志或命令的函数原型 ****/
/* 创建新文件或重写一个已存在的文件 */
extern int creat(const char * filename,mode_t mode);

/* 文件句柄操作 */
extern int fcntl(int fildes,int cmd, ...);

/* 打开文件 */
extern int open(const char * filename, int flags, ...);

#endif
