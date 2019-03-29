#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h>	/* I know - shouldn't do this, but .. */

struct utimbuf {
	time_t actime;		/* 文件访问时间。从1970.1.1:0:0:0开始的秒数 */
	time_t modtime;		/* 文件修改时间。同上 */
};

/* 设置文件访问和修改时间函数 */
extern int utime(const char *filename, struct utimbuf *times);

#endif
