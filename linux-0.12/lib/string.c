/*
 *  linux/lib/string.c
 *
 *  (C) 1991  Linus Torvalds
 */

#ifndef __GNUC__                /* 需要GNU的C编译器编译 */
#error I want gcc!
#endif

#define static
#define inline
#define __LIBRARY__
#include <string.h>

