/*
 * 'kernel.h' contains some often-used function prototypes etc
 */
void verify_area(void * addr, int count);/* 验证给定地址开始的内在块是否超限，若超限则追加内存 */
volatile void panic(const char * str);  /* 显示内核出错信息，然后进入死循环 */
volatile void do_exit(long error_code); /* 进程退出处理 */
int printf(const char * fmt, ...);      /* 标准打印显示函数 */
int printk(const char * fmt, ...);      /* 内核专用的打印信息函数 */
void console_print(const char * str);   /* 控制台显示函数 */
int tty_write(unsigned ch,char * buf,int count);/* 往tty上写指定长度的字符串 */
void * malloc(unsigned int size);       /* 通用内核内存分配函数 */
void free_s(void * obj, int size);      /* 释放指定对象占用的内存 */
extern void hd_times_out(void);         /* 硬盘处理超时 */
extern void sysbeepstop(void);          /* 停止蜂鸣 */
extern void blank_screen(void);         /* 黑屏处理 */
extern void unblank_screen(void);       /* 恢复被黑屏的屏幕 */

extern int beepcount;       /* 蜂鸣时间滴答计数 */
extern int hd_timeout;      /* 硬盘超时滴答值 */
extern int blankinterval;   /* 设定的屏幕黑屏间隔时间 */
extern int blankcount;      /* 黑屏时间计数 */

#define free(x) free_s((x), 0)

/*
 * This is defined as a macro, but at some point this might become a
 * real subroutine that sets a flag if it returns true (to do
 * BSD-style accounting where the process is flagged if it uses root
 * privs).  The implication of this is that you should do normal
 * permissions checks first, and check suser() last.
 */
/*
 * 下面函数是以宏的形式定义的，但是在某方面来看它可以成为一个真正的子程序，如果返回是true时它
 * 将设置标志(如果使用root用户权限的进程设置了标志，则用于执行BSD方式的讨账处理)。这意味着你
 * 应该首先执行常规权限检查，最后再检测suser()。
 */

#define suser() (current->euid == 0)    /* 检测是否为超级用户 */

