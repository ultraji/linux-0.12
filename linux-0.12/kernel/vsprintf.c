/*
 *  linux/kernel/vsprintf.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <stdarg.h>
#include <string.h>

/* we use this so that we can do without the ctype library */
/* 我们使用下面的定义,这样我们就可以不使用ctype库了 */
#define is_digit(c)	((c) >= '0' && (c) <= '9')

/**
 * 数字字符串转换成整数
 * @param[in] 	**s 	指向数字字符串指针的指针
 * @retval		结果数值(另外，指针将前移)
 */
static int skip_atoi(const char **s)
{
	int i=0;

	while (is_digit(**s)) {
		i = i*10 + *((*s)++) - '0';
	}
	return i;
}

/* 转换类型的各种符号常数 */
#define ZEROPAD	1		/* pad with zero */ /* 填充零 */
#define SIGN	2		/* unsigned/signed long */ /* 无符号/符号长整数 */
#define PLUS	4		/* show plus */ /* 显示加 */
#define SPACE	8		/* space if plus */ /* 如是加号,则置空格 */
#define LEFT	16		/* left justified */ /* 左对齐 */
#define SPECIAL	32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF' */ /* 使用小写字母 */

/**
 * 除操作
 * @param[in/out]	n		被除数
 * @param[in]		base	除数
 * @retval			函数返回余数(同时，n为商) */
#define do_div(n, base) ({ 													\
	int __res;																\
	__asm__("divl %4"														\
		:"=a" (n),"=d" (__res)												\
		:"0" (n),"1" (0),"r" (base));										\
	__res; })


/**
 * 将整数转换为指定进制的字符串
 * @param[out]	*str		转换后的字符串
 * @param[in]	num			整数
 * @param[in]	base		进制
 * @param[in]	size		字符串长度
 * @param[in]	precision 	数字长度(精度)
 * @param[in]	type		类型选项
 * @retval		数字转换成字符串后指向该字符串末端后面的指针
 */
static char * number(char * str, int num, int base, int size, int precision
	,int type)
{
	char c, sign, tmp[36];
	const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	if (type & SMALL) {	/* 小写字母集 */
		digits = "0123456789abcdefghijklmnopqrstuvwxyz";
	}
	if (type & LEFT) {	/* 左调整(靠左边界),则屏蔽类型中的填零标志 */
		type &= ~ZEROPAD;
	}
	if (base < 2 || base > 36) {	/* 本程序只能处理基数在2-36之间的数 */
		return 0;
	}
	c = (type & ZEROPAD) ? '0' : ' ';
	if (type&SIGN && num<0) {
		sign = '-';
		num = -num;
	} else {
		sign = (type&PLUS) ? '+' : ((type&SPACE) ? ' ' : 0);
	}
	if (sign){
		size--;
	}
	if (type&SPECIAL) {
		if (base==16) {
			size -= 2;
		} else if (base==8) {
			size--;
		}
	}
	i = 0;
	if (num == 0) {
		tmp[i++]='0';
	} else {
		while (num != 0) {
			tmp[i++] = digits[do_div(num, base)];
		}
	}
	if (i > precision){
		precision = i;
	}
	size -= precision;
	if (!(type & (ZEROPAD+LEFT))) {
		while(size-->0) {
			*str++ = ' ';
		}
	}
	if (sign) {
		*str++ = sign;
	}
	if (type & SPECIAL) {
		if (base==8) {
			*str++ = '0';
		} else if (base==16) {
			*str++ = '0'; 
			*str++ = digits[33];
		}
	}
	if (!(type&LEFT)) {
		while(size-->0) {
			*str++ = c;
		}
	}
	while(i<precision--) {
		*str++ = '0';
	}
	while(i-->0) {
		*str++ = tmp[i];
	}
	while(size-->0) {
		*str++ = ' ';
	}
	return str;
}


/**
 * 送格式化输出到字符串buf中
 * @param[out]		buf		输出字符串缓冲区
 * @param[in]		fmt		格式字符串
 * @param[in]		args	个数变化的值
 * @return		返回字符串buf的长度
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	int i;
	char * str;
	char *s;
	int *ip;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {
			*str++ = *fmt;
			continue;
		}
			
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}
		
		/* get field width */
		field_width = -1;
		if (is_digit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (is_digit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		switch (*fmt) {
		case 'c':
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			break;

		case 's':
			s = va_arg(args, char *);
			len = strlen(s);
			if (precision < 0)
				precision = len;
			else if (len > precision)
				len = precision;

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			break;

		case 'o':
			str = number(str, va_arg(args, unsigned long), 8,
				field_width, precision, flags);
			break;

		case 'p':
			if (field_width == -1) {
				field_width = 8;
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			break;

		case 'x':
			flags |= SMALL;
		case 'X':
			str = number(str, va_arg(args, unsigned long), 16,
				field_width, precision, flags);
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			str = number(str, va_arg(args, unsigned long), 10,
				field_width, precision, flags);
			break;

		case 'n':
			ip = va_arg(args, int *);
			*ip = (str - buf);
			break;

		default:
			if (*fmt != '%') {
				*str++ = '%';
			}
			if (*fmt) {
				*str++ = *fmt;
			} else {
				--fmt;
			}
			break;
		}
	}
	*str = '\0';
	return str-buf;
}
