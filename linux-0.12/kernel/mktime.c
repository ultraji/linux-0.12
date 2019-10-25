/*
 *  linux/kernel/mktime.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <time.h>

/*
 * This isn't the library routine, it is only used in the kernel.
 * as such, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for the library
 * routines (although I think minix times is public).
 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
 */
/**
 * 这不是库函数，它仅供内核使用。因此我们不关心小于1970年的年份等，但假定一切均很正常。
 * 同样，时间区域TZ问题也先忽略。我们只是尽可能简单地处理问题。最好能找到一些公开的库函数
 * （尽管我认为minix的时间函数是公开的）。
 * 另外，我恨那个设置1970年开始的人 - 难道他们就不能选择从一个闰年开始？我恨格里高利历、
 * 罗马教皇、主教，我什么都不在乎。我是个脾气暴躁的人。
 */

#define MINUTE 60
#define HOUR (60*MINUTE)
#define DAY (24*HOUR)
#define YEAR (365*DAY)

/* interestingly, we assume leap-years */
/* 以闰年为基础，每个月开始时的秒数时间 */
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

/**
 * 计算从1970年1月1日0时起到开机当日经过的秒数
 * @param[in]	tm	当前时间
 * @return		返回从1970年1月1日0时起到开机当日经过的秒数	
 */
long kernel_mktime(struct tm * tm)
{
	long res;
	int year;

	/* tm_year是2位表示方式，处理2000年的问题（例如：2018=>18+100-70） */
	if (tm->tm_year < 70)
		tm->tm_year += 100;

	year = tm->tm_year - 70;
/* magic offsets (y+1) needed to get leapyears right.*/
/* 由于UNIX计年份y是从1970年算起。到1972年就是一个闰年，因此过3年才收到了第1个闰年的影响，
 即从1970年到当前年份经过的闰年数为(y+1)/4 */
	res = YEAR * year + DAY * ((year+1)/4);
	
	res += month[tm->tm_mon];
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
/* 如果月份大于2月，则需要判断当前年份是否为闰年，计算公式为(year+2)%4。非闰年则需要减去1
 天，因为month[]这个数组以闰年为假设的 */
	if (tm->tm_mon>1 && ((year+2)%4)) { 
		res -= DAY;
	}
	res += DAY * (tm->tm_mday - 1);
	res += HOUR * tm->tm_hour;
	res += MINUTE * tm->tm_min;
	res += tm->tm_sec;
	return res;
}
