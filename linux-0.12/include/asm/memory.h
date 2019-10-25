/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 */
/*
 * 注意！！！memcpy(dest, src, n) 假设段寄存器ds=es=通常数据段。在内核中使用的所有函数都基于假
 * 设（ds=es=内核空间，fs=局部数据空间，gs=null），具有良好行为的应用程序也是这样（ds=es=用户数
 * 据空间）。如果任何用户程序随意改动了es寄存器而出错，则并不是由于系统程序错误造成的。
 */
/**
 * 内存块复制
 * 从源地址src处开始复制n个字节到目的地址dest处。从ds:[esi]复制到es:[edi]，共复制ecx(n)字节。
 * @param[in]	dest	复制的目的地址
 * @param[in]	src		复制的源地址
 * @param[in]	n		复制字节数
 */
#define memcpy(dest, src, n) ({ 										\
	void * _res = dest;													\
	__asm__ ("cld;rep;movsb"											\
		::"D" ((long)(_res)),"S" ((long)(src)),"c" ((long) (n))			\
		);																\
	_res;																\
})
