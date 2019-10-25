include Rules.make

#
# if you want the ram-disk device, define this to be the size in blocks.
#
RAMDISK = #-DRAMDISK=512

# -0: 使用16bit代码段　
# -a: 开启与GNU as，ld部分兼容性选项
AS86	= $(QUIET_CC)as86 -0 -a

# -0: 产生具有16bit魔数的头结构
LD86	= $(QUIET_LINK)ld86 -0

# -s(去除): 输出文件中省略所有的符号信息
# -x: 删除所有局部符号
# -M: 用于输出符号表
# -e startup_32(新增): 指定入口
# -Ttext 0(新增): 使`startup_32`标号对应的地址为`0x0`
LDFLAGS	= -M -x -Ttext 0 -e startup_32

CC	+= $(RAMDISK)

# -g: 生成调试信息
# -Wall: 打印警告
# -O: 对代码进行优化
# -fstrength-reduce: 优化循环语句
# -fomit-frame-pointer: 对无需帧指针的函数不要把帧指针保留在寄存器中
# -fcombine-regs(去除): 不再被gcc支持
# -mstring-insns(去除): Linus本人增加的选项(gcc中没有)
# -fno-builtin(新增): 阻止gcc会把没有参数的printf优化成puts
CFLAGS	= -g -Wall -O -fstrength-reduce -fomit-frame-pointer -fno-builtin

# -Iinclude: 通过-I指定在该项目的include文件夹中搜索
CPP	+= -Iinclude

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.
#
ROOT_DEV= #/dev/hd6
SWAP_DEV= #/dev/hd2

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS =kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH	=kernel/math/math.a
LIBS	=lib/lib.a

.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<

all:	Image

Image: boot/bootsect boot/setup tools/system tools/build
	@cp -f tools/system system.tmp
	@strip system.tmp
	@objcopy -O binary -R .note -R .comment system.tmp tools/kernel
	tools/build boot/bootsect boot/setup tools/kernel $(ROOT_DEV) $(SWAP_DEV) > Image
	@rm system.tmp
	@rm tools/kernel -f
	@sync

# 上面的修改就可以保证开启gcc调试后，System文件不变大，build也就不会失败了(build对system大小有
# 限制)。原先规则为下面两句
# tools/build boot/bootsect boot/setup tools/system $(ROOT_DEV) $(SWAP_DEV) > Image
# sync

disk: Image
	dd bs=8192 if=Image of=/dev/PS0

tools/build: tools/build.c
	$(CC) $(CFLAGS) \
	-o tools/build tools/build.c

boot/head.o: boot/head.s

tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system > System.map

kernel/math/math.a:
	@(cd kernel/math; make)

kernel/blk_drv/blk_drv.a:
	@(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv.a:
	@(cd kernel/chr_drv; make)

kernel/kernel.o:
	@(cd kernel; make)

mm/mm.o:
	@(cd mm; make)

fs/fs.o:
	@(cd fs; make)

lib/lib.a:
	@(cd lib; make)

boot/setup: boot/setup.s
	$(AS86) -o boot/setup.o boot/setup.s
	$(LD86) -s -o boot/setup boot/setup.o

boot/setup.s:	boot/setup.S include/linux/config.h
	$(CPP) -traditional boot/setup.S -o boot/setup.s

boot/bootsect.s:	boot/bootsect.S include/linux/config.h
	$(CPP) -traditional boot/bootsect.S -o boot/bootsect.s

boot/bootsect:	boot/bootsect.s
	$(AS86) -o boot/bootsect.o boot/bootsect.s
	$(LD86) -s -o boot/bootsect boot/bootsect.o

clean:
	rm -f Image System.map tmp_make core boot/bootsect boot/setup \
		boot/bootsect.s boot/setup.s
	rm -f init/*.o tools/system tools/build boot/*.o
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

backup: clean
	(cd .. ; tar cf - linux | compress - > backup.Z)
	sync

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

### Dependencies:
init/main.o : init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/time.h include/time.h include/sys/times.h \
  include/sys/utsname.h include/sys/param.h include/sys/resource.h \
  include/utime.h include/linux/tty.h include/termios.h include/linux/sched.h \
  include/linux/head.h include/linux/fs.h include/linux/mm.h \
  include/linux/kernel.h include/signal.h include/asm/system.h \
  include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h \
  include/string.h 
