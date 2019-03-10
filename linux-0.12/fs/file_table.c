/*
 *  linux/fs/file_table.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/fs.h>

/* 文件表数组(64项) */
struct file file_table[NR_FILE];
