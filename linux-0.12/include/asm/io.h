/* 该文件中定义了对硬件IO端口访问的嵌入式汇编宏函数：outb()、inb()、outb_p()和inb_p() */

/** 
 * 硬件端口字节输出
 * @param[in]	value	欲输出字节
 * @param[in]	port	端口
 */
#define outb(value, port) \
	__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

/** 
 * 硬件端口字节输入
 * @param[in]	port	端口
 * @retval		返回读取的字节
 */
#define inb(port) ({ 												\
	unsigned char _v; 												\
	__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port));		\
	_v; 															\
	})

/**
 * 硬件端口字节输出(带延迟)
 * 使用两条跳转语句来延迟一会儿
 * @param[in]	value	欲输出字节
 * @param[in]	port	端口
 */
#define outb_p(value, port) 										\
	__asm__ ("outb %%al,%%dx\n"										\
			"\tjmp 1f\n"											\
			"1:\tjmp 1f\n" 											\
			"1:"::"a" (value),"d" (port))

/**
 * 硬件端口字节输入(带延迟)
 * 使用两条跳转语句来延迟一会儿
 * @param[in]	port	端口
 * @retval		返回读取的字节
 */
#define inb_p(port) ({												\
	unsigned char _v;												\
	__asm__ volatile (												\
		"inb %%dx,%%al\n"											\
		"\tjmp 1f\n"												\
		"1:\tjmp 1f\n"												\
		"1:":"=a" (_v):"d" (port));									\
	_v; 															\
	})
