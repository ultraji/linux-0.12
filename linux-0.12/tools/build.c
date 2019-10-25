/*
 *  linux/tools/build.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * This file builds a disk-image from three different files:
 *
 * - bootsect: max 510 bytes of 8086 machine code, loads the rest
 * - setup: max 4 sectors of 8086 machine code, sets up system parm
 * - system: 80386 code for actual system
 *
 * It does some checking that all files are of the correct type, and
 * just writes the result to stdout, removing headers and padding to
 * the right amount. It also writes some system data to stderr.
 */
/*
 * 该程序从三个不同的程序中创建磁盘映像文件:
 * 
 * - bootsect:	该文件的8086机器码最长为510字节，用于加载其他程序。
 * - setup:		该文件的8086机器码最长为4个磁盘扇区，用于设置系统参数。
 * - system:	实际系统的80386代码。
 * 
 * 该程序首先检查所有程序模块的类型是否正确，并将检查结果在终端上显示出来，然后删除模块头部并扩
 * 充到正确的长度。该程序也会将一些系统数据写到stderr。
 */
/*
 * Changes by tytso to allow root device specification
 *
 * Added swap-device specification: Linux 20.12.91
 */
/*
 * tytso对该程序作了修改，以允许指定根文件设备。
 * 添加了指定交换设备功能：Linus 20.12.91 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef MAJOR
	#define MAJOR(a) (((unsigned)(a))>>8)
#endif
#ifndef MINOR
	#define MINOR(a) ((a)&0xff)
#endif

#define MINIX_HEADER 	32		/* minix二进制目标文件模块头部长度为32B */
#define GCC_HEADER 		1024	/* GCC头部信息长度为1024B */

#define SYS_SIZE 		0x3000	/* system文件最长节数(SYS_SIZE*16=128KB) */

// 默认地把Linux根文件系统所在的设备设置为在第2个硬盘的第1个分区上(即设备号为0x0306)，是因为
// Linus当时开发Linux时，把第1个硬盘用作MINIX系统盘，而第2个硬盘用作Linux的根文件系统盘。

#define DEFAULT_MAJOR_ROOT 3	/* 默认根设备主设备号 - 3(硬盘) */
#define DEFAULT_MINOR_ROOT 1//6	/* 默认根设备次设备号 - 6(第2个硬盘的第1分区) */

#define DEFAULT_MAJOR_SWAP 3	/* 默认交换设备主设备号 */
#define DEFAULT_MINOR_SWAP 4	/* 默认交换设备次设备号 */

/* max nr of sectors of setup: don't change unless you also change bootsect etc */
/* 下面指定setup模块占的最大扇区数：不要改变该值，除非也改变bootsect等相应文件 */
#define SETUP_SECTS 4           /* setup最大长度4个扇区(2KB) */

#define STRINGIFY(x) #x         /* 把x转换成字符串类型 */

/* 显示出错信息，并终止程序 */
void die(char * str)
{
	fprintf(stderr, "%s\n", str);
	exit(1);
}

/* 显示程序使用方法，并退出 */
void usage(void)
{
	die("Usage: build bootsect setup system [rootdev] [> image]");
}

/* 主程序 */
int main(int argc, char ** argv)
{
	int i, c, id;
	char buf[1024];
	char major_root, minor_root;
	char major_swap, minor_swap;
	struct stat sb;

	/* build程序命令行参数不是4-6个(程序名算作1个)，则显示程序用法并退出 */
	if ((argc < 4) || (argc > 6)) {
		usage();
	}
	/* 若argc>4，如果根设备名不是软盘("FLOPPY")，则取该设备文件的状态信息。如果根设备就
	 是FLOPPY设备，则让主设备号取0 */
	if (argc > 4) {
		if (strcmp(argv[4], "FLOPPY")) {
			if (stat(argv[4], &sb)) {
				perror(argv[4]);
				die("Couldn't stat root device.");
			}
			major_root = MAJOR(sb.st_rdev);	/* 取设备名状态结构中设备号 */
			minor_root = MINOR(sb.st_rdev);
		} else {
			major_root = 0;
			minor_root = 0;
		}
	/* 若argc=4，则取系统默认的根设备号 */
	} else {
		major_root = DEFAULT_MAJOR_ROOT;
		minor_root = DEFAULT_MINOR_ROOT;
	}
	/* 若argc=6，如果交换设备不是"NONE"，则取设备名状态结构中的设备号。如果交换设备就是"NONE"，
	 则让交换设备的主设备号和次设备号取为0，表示交换设备就是当前启动引导设备 */
	if (argc == 6) {
		if (strcmp(argv[5], "NONE")) {
			if (stat(argv[5], &sb)) {
				perror(argv[5]);
				die("Couldn't stat root device.");	/* root好像应该改成swap */
			}
			major_swap = MAJOR(sb.st_rdev);         /* 取设备名状态结构中设备号 */
			minor_swap = MINOR(sb.st_rdev);
		} else {
			major_swap = 0;
			minor_swap = 0;
		}
	} else {
	/* 若argc=5，表示命令行上没有交换设备名，则取系统默认的交换设备号 */
		major_swap = DEFAULT_MAJOR_SWAP;
		minor_swap = DEFAULT_MINOR_SWAP;
	}
	fprintf(stderr, "Root device is (%d, %d)\n", major_root, minor_root);
	fprintf(stderr, "Swap device is (%d, %d)\n", major_swap, minor_swap);
	/* 如果主设备号不等于2(软盘)或3(硬盘)也不为0(取系统默认设备)，则显示出错信息并退出 */
	if ((major_root != 2) && (major_root != 3) && (major_root != 0)) {
		fprintf(stderr, "Illegal root device (major = %d)\n", major_root);
		die("Bad root device --- major #");
	}
	if (major_swap && major_swap != 3) {
		fprintf(stderr, "Illegal swap device (major = %d)\n", major_swap);
		die("Bad root device --- major #");
	}
	/* 首先初始化1KB的复制缓冲区，置全0 */
	for (i = 0; i < sizeof buf; i++) {
		buf[i] = 0;
	}

/**** bootsect ****/
	if ((id = open(argv[1], O_RDONLY, 0)) < 0) {
		die("Unable to open 'boot'");
	}
	/* 从中读取MINIX执行文件头结构内容到缓冲区buf中。bootsect = 32字节的头部+512字
	 节的引导扇区代码和数据 */
	if (read(id, buf, MINIX_HEADER) != MINIX_HEADER) {
		die("Unable to read header of 'boot'");
	}
	/* 接下来根据MINIX头部结构判断bootsect是否为一个有效的MINIX执行文件。
	 0x0301 - a_magic MINIX头部魔数
	 0x10 - a_flag 可执行
	 0x04 - a_cpu Intel8086机器码 */
	if (((long *) buf)[0] != 0x04100301) {
		die("Non-Minix header of 'boot'");
	}
	if (((long *) buf)[1] != MINIX_HEADER) { /* 头部长度字段 a_hdrlen */
		die("Non-Minix header of 'boot'");
	}
	if (((long *) buf)[3] != 0) { /* 数据段长字段 a_data */
		die("Illegal data segment in 'boot'");
	}
	if (((long *) buf)[4] != 0) { /* 堆字段 a_bss */
		die("Illegal bss in 'boot'");
	}
	if (((long *) buf)[5] != 0) { /* 执行点字段 a_entry */
		die("Non-Minix header of 'boot'");
	}
	if (((long *) buf)[7] != 0) { /* 符号表长字段 a_sym */
		die("Illegal symbol table in 'boot'");
	}
	i = read(id, buf, sizeof buf);
	fprintf(stderr, "Boot sector is %d bytes.\n", i);
	/* 512字节的引导扇区代码和数据 */
	if (i != 512) {
		die("Boot block must be exactly 512 bytes");
	}
	/* 最后2字节应该是引导标志0xAA55 */
	if ((*(unsigned short *)(buf+510)) != 0xAA55) {
		die("Boot block hasn't got boot flag (0xAA55)");
	}
	/* 引导扇区的506， 507偏移处需存放交换设备号；508，509偏移处需存放根设备号 */
	buf[506] = (char) minor_swap;
	buf[507] = (char) major_swap;
	buf[508] = (char) minor_root;
	buf[509] = (char) major_root;
	
	/* 在linux/Makefile中，build程序标准输出被重定向到内核映像文件名Image上，因此引
	导扇区代码和数据会被写到Image开始的512字节处 */
	i = write(1, buf, 512);
	if (i != 512){
		die("Write call failed");
	}
	close (id);

/**** setup模块 ****/
	if ((id = open(argv[2], O_RDONLY, 0)) < 0) {
		die("Unable to open 'setup'");
	}
	/* 读取32字节的MINIX执行文件头结构内容到缓冲区buf中 */
	if (read(id, buf, MINIX_HEADER) != MINIX_HEADER) {
		die("Unable to read header of 'setup'");
	}
	/* 接下来根据MINIX头部结构判断setup是否为一个有效的MINIX执行文件
	0x0301 - a_magic，MINIX头部魔数
	0x10 - a_flag，可执行
	0x04 - a_cpu, Intel8086机器码 */
	if (((long *) buf)[0] != 0x04100301) {
		die("Non-Minix header of 'setup'");
	}
	if (((long *) buf)[1] != MINIX_HEADER) { 	/* 头部长度字段a_hdrlen */
		die("Non-Minix header of 'setup'");
	}      
	if (((long *) buf)[3] != 0) {		/* 数据段长字段 a_data */
		die("Illegal data segment in 'setup'");
	}
	if (((long *) buf)[4] != 0) {		/* 堆字段 a_bss */
		die("Illegal bss in 'setup'");
	}
	if (((long *) buf)[5] != 0) {		/* 执行起始点字段 a_entry */
		die("Non-Minix header of 'setup'");
	}
	if (((long *) buf)[7] != 0) {		/* 表长字段 a_sym */
		die("Illegal symbol table in 'setup'");
	}
	/* 同时统计写的长度(i)，该值不能大于(SETUP_SECTS * 512) */
	for (i = 0; (c = read(id, buf, sizeof buf)) > 0; i += c ) {
		if (write(1, buf, c) != c) {
			die("Write call failed");
		}
	}
	close (id);
	if (i > SETUP_SECTS * 512) {
		die("Setup exceeds " STRINGIFY(SETUP_SECTS)  \
			" sectors - rewrite build/boot/setup");
	}
	fprintf(stderr, "Setup is %d bytes.\n", i);
	for (c = 0; c < sizeof(buf); c++) {
		buf[c] = '\0';	/* buf数组清零 */
	}
	/* 用NULL字符将setup填足为4*512B */
	while (i < SETUP_SECTS * 512) {
		c = SETUP_SECTS * 512 - i;
		if (c > sizeof(buf)) {
			c = sizeof(buf);
		}
		if (write(1, buf, c) != c) {
			die("Write call failed");
		}
		i += c;
	}

/**** system模块 ****/
	/* system模块使用gas编译，具有GNU a.out目标文件格式 */
	if ((id = open(argv[3], O_RDONLY, 0)) < 0) {
		die("Unable to open 'system'");
	}
	#if 0
	/** 
	 * 去掉这里的原因是
	 * 1. 去除a.out格式头部的动作已经在主目录Makefile中进行了，故在这里注释掉。 
	 * 2. 入口位置不应该是((long *) buf)[5]，应该为((long *) buf)[6]，可以在linux下，通过
	 *  命令 readelf -h system 和 od -w4 -N 80 -x system 对比看到入口地址应该在第28~31个
	 *  字节处。
	 */
	if (read(id, buf, GCC_HEADER) != GCC_HEADER){
		die("Unable to read header of 'system'");
	}
	if (((long *) buf)[6/*5*/] != 0){      /* 执行入口点字段 a_entry值应为 0 */
		die("Non-GCC header of 'system'");
	}
	#endif
	for (i = 0; (c = read(id, buf, sizeof buf)) > 0; i += c ) {
		if (write(1, buf, c) != c) {
			die("Write call failed");
		}
	}
	close(id);
	fprintf(stderr, "System is %d bytes.\n", i);
	/* 若system模块超过SYS_SIZE(即128KB)，则显示出错信息并退出 */
	if (i > SYS_SIZE * 16) {
		die("System is too big");
	}
	return(0);
}
