!
!	setup.s		(C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!
;  setup.s负责从BIOS中获取系统数据，并将这些数据放到系统内存的适当地方。
; 此时setup.s和system已经由bootsect引导块加载到内存中。
;
; 这段代码询问bios有关内存/磁盘/其它参数，并将这些参数放到一个“安全的”地
; 方：0x90000 - 0x901FF，也即原来bootsect代码块曾经在的地方，然后在被缓冲
; 块覆盖掉之前由保护模式的system读取。
!

! NOTE! These had better be the same as in bootsect.s!
; 以下这些参数最好和bootsect.s中的相同！
#include <linux/config.h>
; config.h中定义了DEF_INITSEG = 0x9000，DEF_SYSSEG = 0x1000，DEF_SETUPSEG = 0x9020。
INITSEG  = DEF_INITSEG	! we move boot here - out of the way
SYSSEG   = DEF_SYSSEG	! system loaded at 0x10000 (65536).
SETUPSEG = DEF_SETUPSEG	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.
; ok，整个读磁盘过程都正常，现在将光标位置保存以备今后使用。

	mov	ax,#INITSEG		! this is done in bootsect already, but...
						; 这已经在bootsect程序中设置过ds，但是...
	mov	ds,ax

;;;;; 1.获取系统初始化所需要的参数 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 内存地址	 ; 字节 ; 内容				 ;
; 0x90000	; 2	;   光标位置			;
; 0x90002	; 2	;	扩展内存大小 		 ;
; 0x90004	; 2	;	显示页面			;
; 0x90006	; 1 ;	显示模式			;
; 0x90007	; 1 ;	字符列数			;
; 0x90008	; 2	;	??				   ;
; 0x9000A	; 1	;	安装的显示内存		 ;
; 0x9000B	; 1	;	显示状态(彩色/单色)	 ;
; 0x9000C	; 2	;	显示卡特性参数		 ;
; 0x9000E	; 1	;	屏幕当前行值		 ;
; 0x9000F	; 1	;	屏幕当前列值		 ;
; ...								   ;
; 0x90080	; 16;	第1个硬盘的参数表	  ;
; 0x90090	; 16;	第2个硬盘的参数表	  ;
; 0x901FC	; 2 ;	根文件系统所在的设备号（bootsec.s中设置）;

! Get memory size (extended mem, kB) 
;;; 取从0x100000（1M）处开始的扩展内存大小（KB）
	mov	ah,#0x88
	int	0x15
	mov	[2],ax

! check for EGA/VGA and some config parameters 
;;; 检查EGA/VGA和一些配置参数
	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx
	mov	[12],cx
	; 检测屏幕当前行列值。若显示卡是VGA卡，则请求用户选择显示行列值
	mov	ax,#0x5019  ; 预设行列值(ah = 80列，al = 25行)
	cmp	bl,#0x10 
	je	novga
	call	chsvga
novga:	
	mov	[14],ax	
;;; 获取光标位置
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! it from 0x90000.
	
! Get video-card data:
;;; 取显示卡当前显示模式
	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

;;; 复制两个硬盘的参数表：
! Get hd0 data
	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]			; 取中断向量0x41的值，即hd0参数表的地址--ds:si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080			; 传输目的地址: 0x9000:0x0080 --es:di
	mov	cx,#0x10
	rep
	movsb

! Get hd1 data
	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]			; 取中断向量0x46的值，即hd1参数表的地址--ds:si
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090			; 传输目的地址: 0x9000:0x0090 --es:di
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)
;;; 检查系统是否存在第2个硬盘，如果不存在则第2个表清零。

	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3
	je	is_disk1		; 是硬盘吗?(类型 = 3?).
no_disk1:
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

! now we want to move to protected mode ...
; 这里开始，我们将要开始进入保护模式

	cli			! no interrupts allowed !		; 这里开始不允许任何中断

! first we move the system to it's rightful place
;;;;; 2. 把整个system模块移动到0x00000位置 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	mov	ax,#0x0000
	cld					! 'direction' = 0, movs moves forward
do_move:
	mov	es,ax			! destination segment ; es:di是目的地址(初始为0x0:0x0)
	add	ax,#0x1000
	cmp	ax,#0x9000		; 已经把最后一段(从0x8000段开始的64KB)代码移动完.
	jz	end_move 		; 判断是否移动完成
	mov	ds,ax			! source segment
	sub	di,di
	sub	si,si
	mov cx,#0x8000 		; 移动0x8000个字
	rep
	movsw
	jmp	do_move

! then we load the segment descriptors
;;;;; 3. 此后，我们加载段描述符表（临时gdt，ldt表） ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

end_move:
	mov	ax,#SETUPSEG	! right, forgot this at first. didn't work :-)
	mov	ds,ax
	lidt	idt_48		! load idt with 0,0					 ; 加载IDT寄存器
	lgdt	gdt_48		! load gdt with whatever appropriate ; 加载GDT寄存器

! that was painless, now we enable A20
;;;;; 为了能访问1MB以上的内存，需要开启A20地址线 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	call	empty_8042
	mov	al,#0xD1		! command write ; 0xD1 - 写数据到P2口
	out	#0x64,al 		; 位1用于A20线的选通
	call	empty_8042
	mov	al,#0xDF		! A20 on
	out	#0x60,al
	call	empty_8042

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.
; 希望以上一切正常。现在我们必须重新对中断进行编程 :-(
; 我们将它们放在正好处于intel保留的硬件中断后面，在int 0x20 - 0x2F。
; 在那里它们不会引起冲突。不幸的是IBM在原PC机中搞糟了，以后也没有纠正过来。
; PC机的BIOS将中断放在了0x08-0x0f，这些中断也被用于内部硬件中断。
; 所以我们就必须重新对8259中断控制器进行编程，这一点都没意思。

;;;;; 以下是重新对8259A芯片进行编程，不是需要理解的重点，不做具体介绍 ;;;;;;;;;;;;;;;;;
	mov	al,#0x11		! initialization sequence
	out	#0x20,al		! send it to 8259A-1
	.word	0x00eb,0x00eb		! jmp $+2, jmp $+2 	; $ 表示当前指令的地址，
	out	#0xA0,al		! and to 8259A-2
	.word	0x00eb,0x00eb
	mov	al,#0x20		! start of hardware int's (0x20)
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x28		! start of hardware int's 2 (0x28)
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x04		! 8259-1 is master
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x02		! 8259-2 is slave
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x01		! 8086 mode for both
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0xFF		! mask off all interrupts for now
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.
; 哼，上面这段当然没劲:-(，希望这样能工作，而且我们也不再需要乏味的BIOS了（除了
; 初始的加载:-)。BIOS子程序要求很多不必要的数据，而且它一点都没趣。那是“真正”的
; 程序员所做的事。
; 进入保护模式时，我们只需要简单地跳转到绝对地址0x00000处就好。 
; 这里设置进入32位保护模式运行。首先加载机器状态字(lmsw - Load Machine Status 
; Word)，也称控制寄存器CR0，将其位0置1将导致CPU工作在保护模式。
; 注：在Intel公司的手册上建议80386或以上CPU应该使用指令“mov cr0,ax”切换到保护模式。

;;;;; 4. 开启保护模式 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	mov	ax,#0x0001	! protected mode (PE) bit
	lmsw	ax		! This is it!				; 将CR0的位0置1开启保护模式
	jmpi	0,8		! jmp offset 0 of segment 8 (cs)
	; 偏移地址是0，因为system模块已被移动到0x00000处了。
	; 段值8则是保护模式下的段选择符。
	; 段选择符长度为16位（2字节）：
	; 	位0-1	- 表示请求的特权级0-3 ，linux0.12只用到两级：0级（系统级）和3级（用户级）
	;	位2		- 用于选择全局描述符表(0)还是局部描述符表(1)
	; 	位3-15	- 描述符表项的索引
	; 段选择符8(0000,0000,0000,1000)表示请求特权级0、使用全局描述符表gdt中的第1项（该项指出代码
	; 的基地址是0），因此这里的跳转指令就会去执行system中的代码。

;;;;;;;;;;;;;;;; setup程序到这就结束了 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway. 				'
empty_8042:
	.word	0x00eb,0x00eb
	in	al,#0x64		! 8042 status port		; 读AT键盘控制器状态寄存器
	test	al,#2		! is input buffer full? ; 测试位1,输入缓冲器满?
	jnz	empty_8042		! yes - loop
	ret

! Routine trying to recognize type of SVGA-board present (if any)
! and if it recognize one gives the choices of resolution it offers.
! If one is found the resolution chosen is given by al,ah (rows,cols).

;;;;; 以下代码牵涉到众多显卡端口信息，比较复杂。但跟内核运行关系不大，所以可以跳过不看 ;;;
chsvga:	cld
	push	ds
	push	cs
	pop	ds
	mov 	ax,#0xc000
	mov	es,ax
	lea	si,msg1
	call	prtstr
nokey:	
	in	al,#0x60
	cmp	al,#0x82
	jb	nokey
	cmp	al,#0xe0
	ja	nokey
	cmp	al,#0x9c
	je	svga
	mov	ax,#0x5019
	pop	ds
	ret
svga:	lea 	si,idati		! Check ATI 'clues'
	mov	di,#0x31
	mov 	cx,#0x09
	repe
	cmpsb
	jne	noati
	lea	si,dscati
	lea	di,moati
	lea	cx,selmod
	jmp	cx
noati:	mov	ax,#0x200f		! Check Ahead 'clues'
	mov	dx,#0x3ce
	out	dx,ax
	inc	dx
	in	al,dx
	cmp	al,#0x20
	je	isahed
	cmp	al,#0x21
	jne	noahed
isahed:	lea	si,dscahead
	lea	di,moahead
	lea	cx,selmod
	jmp	cx
noahed:	mov	dx,#0x3c3		! Check Chips & Tech. 'clues'
	in	al,dx
	or	al,#0x10
	out	dx,al
	mov	dx,#0x104		
	in	al,dx
	mov	bl,al
	mov	dx,#0x3c3
	in	al,dx
	and	al,#0xef
	out	dx,al
	cmp	bl,[idcandt]
	jne	nocant
	lea	si,dsccandt
	lea	di,mocandt
	lea	cx,selmod
	jmp	cx
nocant:	mov	dx,#0x3d4		! Check Cirrus 'clues'
	mov	al,#0x0c
	out	dx,al
	inc	dx
	in	al,dx
	mov	bl,al
	xor	al,al
	out	dx,al
	dec	dx
	mov	al,#0x1f
	out	dx,al
	inc	dx
	in	al,dx
	mov	bh,al
	xor	ah,ah
	shl	al,#4
	mov	cx,ax
	mov	al,bh
	shr	al,#4
	add	cx,ax
	shl	cx,#8
	add	cx,#6
	mov	ax,cx
	mov	dx,#0x3c4
	out	dx,ax
	inc	dx
	in	al,dx
	and	al,al
	jnz	nocirr
	mov	al,bh
	out	dx,al
	in	al,dx
	cmp	al,#0x01
	jne	nocirr
	call	rst3d4	
	lea	si,dsccirrus
	lea	di,mocirrus
	lea	cx,selmod
	jmp	cx
rst3d4:	mov	dx,#0x3d4
	mov	al,bl
	xor	ah,ah
	shl	ax,#8
	add	ax,#0x0c
	out	dx,ax
	ret	
nocirr:	call	rst3d4			! Check Everex 'clues'
	mov	ax,#0x7000
	xor	bx,bx
	int	0x10
	cmp	al,#0x70
	jne	noevrx
	shr	dx,#4
	cmp	dx,#0x678
	je	istrid
	cmp	dx,#0x236
	je	istrid
	lea	si,dsceverex
	lea	di,moeverex
	lea	cx,selmod
	jmp	cx
istrid:	lea	cx,ev2tri
	jmp	cx
noevrx:	lea	si,idgenoa		! Check Genoa 'clues'
	xor 	ax,ax
	seg es
	mov	al,[0x37]
	mov	di,ax
	mov	cx,#0x04
	dec	si
	dec	di
l1:	inc	si
	inc	di
	mov	al,(si)
	seg es
	and	al,(di)
	cmp	al,(si)
	loope 	l1
	cmp	cx,#0x00
	jne	nogen
	lea	si,dscgenoa
	lea	di,mogenoa
	lea	cx,selmod
	jmp	cx
nogen:	lea	si,idparadise		! Check Paradise 'clues'
	mov	di,#0x7d
	mov	cx,#0x04
	repe
	cmpsb
	jne	nopara
	lea	si,dscparadise
	lea	di,moparadise
	lea	cx,selmod
	jmp	cx
nopara:	mov	dx,#0x3c4		! Check Trident 'clues'
	mov	al,#0x0e
	out	dx,al
	inc	dx
	in	al,dx
	xchg	ah,al
	mov	al,#0x00
	out	dx,al
	in	al,dx
	xchg	al,ah
	mov	bl,al		! Strange thing ... in the book this wasn't 				'
	and	bl,#0x02	! necessary but it worked on my card which
	jz	setb2		! is a trident. Without it the screen goes
	and	al,#0xfd	! blurred ...
	jmp	clrb2		!
setb2:	or	al,#0x02	!
clrb2:	out	dx,al
	and	ah,#0x0f
	cmp	ah,#0x02
	jne	notrid
ev2tri:	lea	si,dsctrident
	lea	di,motrident
	lea	cx,selmod
	jmp	cx
notrid:	mov	dx,#0x3cd		! Check Tseng 'clues'
	in	al,dx			! Could things be this simple ! :-)
	mov	bl,al
	mov	al,#0x55
	out	dx,al
	in	al,dx
	mov	ah,al
	mov	al,bl
	out	dx,al
	cmp	ah,#0x55
 	jne	notsen
	lea	si,dsctseng
	lea	di,motseng
	lea	cx,selmod
	jmp	cx
notsen:	mov	dx,#0x3cc		! Check Video7 'clues'
	in	al,dx
	mov	dx,#0x3b4
	and	al,#0x01
	jz	even7
	mov	dx,#0x3d4
even7:	mov	al,#0x0c
	out	dx,al
	inc	dx
	in	al,dx
	mov	bl,al
	mov	al,#0x55
	out	dx,al
	in	al,dx
	dec	dx
	mov	al,#0x1f
	out	dx,al
	inc	dx
	in	al,dx
	mov	bh,al
	dec	dx
	mov	al,#0x0c
	out	dx,al
	inc	dx
	mov	al,bl
	out	dx,al
	mov	al,#0x55
	xor	al,#0xea
	cmp	al,bh
	jne	novid7
	lea	si,dscvideo7
	lea	di,movideo7
selmod:	push	si
	lea	si,msg2
	call	prtstr
	xor	cx,cx
	mov	cl,(di)
	pop	si
	push	si
	push	cx
tbl:	pop	bx
	push	bx
	mov	al,bl
	sub	al,cl
	call	dprnt
	call	spcing
	lodsw
	xchg	al,ah
	call	dprnt
	xchg	ah,al
	push	ax
	mov	al,#0x78
	call	prnt1
	pop	ax
	call	dprnt
	call	docr
	loop	tbl
	pop	cx
	call	docr
	lea	si,msg3
	call	prtstr
	pop	si
	add	cl,#0x80
nonum:	in	al,#0x60	! Quick and dirty...
	cmp	al,#0x82
	jb	nonum
	cmp	al,#0x8b
	je	zero
	cmp	al,cl
	ja	nonum
	jmp	nozero
zero:	sub	al,#0x0a
nozero:	sub	al,#0x80
	dec	al
	xor	ah,ah
	add	di,ax
	inc	di
	push	ax
	mov	al,(di)
	int 	0x10
	pop	ax
	shl	ax,#1
	add	si,ax
	lodsw
	pop	ds
	ret
; 若都不是上面检测地显示卡，我们就只好采用默认地80*25的标准行列值。
novid7:	pop	ds	! Here could be code to support standard 80x50,80x30
	mov	ax,#0x5019	
	ret

! Routine that 'tabs' to next col.

spcing:	mov	al,#0x2e
	call	prnt1
	mov	al,#0x20
	call	prnt1	
	mov	al,#0x20
	call	prnt1	
	mov	al,#0x20
	call	prnt1	
	mov	al,#0x20
	call	prnt1
	ret	

! Routine to print asciiz-string at DS:SI

prtstr:	lodsb
	and	al,al
	jz	fin
	call	prnt1
	jmp	prtstr
fin:	ret

! Routine to print a decimal value on screen, the value to be
! printed is put in al (i.e 0-255). 

dprnt:	push	ax
	push	cx
	mov	ah,#0x00		
	mov	cl,#0x0a
	idiv	cl
	cmp	al,#0x09
	jbe	lt100
	call	dprnt
	jmp	skip10
lt100:	add	al,#0x30
	call	prnt1
skip10:	mov	al,ah
	add	al,#0x30
	call	prnt1	
	pop	cx
	pop	ax
	ret

! Part of above routine, this one just prints ascii al

prnt1:	push	ax
	push	cx
	mov	bh,#0x00
	mov	cx,#0x01
	mov	ah,#0x0e
	int	0x10
	pop	cx
	pop	ax
	ret

! Prints <CR> + <LF>

docr:	push	ax
	push	cx
	mov	bh,#0x00
	mov	ah,#0x0e
	mov	al,#0x0a
	mov	cx,#0x01
	int	0x10
	mov	al,#0x0d
	int	0x10
	pop	cx
	pop	ax
	ret	
;;;;; 跟显示卡相关代码至此结束 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;; 全局描述符表（临时，每个描述符项长8个字节）
gdt:
	.word	0,0,0,0		! dummy	;第1个描述符，不用

	; 在GDT表的偏移量是0x08。它是内核代码段选择符的值。
	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9A00		! code read/exec		; 代码段为只读，可执行
	.word	0x00C0		! granularity=4096, 386 ; 颗粒度4K，32位

	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9200		! data read/write		; 数据段为可读可写
	.word	0x00C0		! granularity=4096, 386	; 颗粒度4K，32位

; 加载中断描述符表寄存器指令lidt要求的6字节操作数。
; 注：CPU要求在进入保护模式之前需设置idt表，因此这里先设置一个长度为0的空表。
idt_48:
	.word	0			! idt limit=0	; idt的限长
	.word	0,0			! idt base=0L	; idt表在线性地址空间中的32位基地址

; 加载全局描述符表寄存器指令lgdt要求的6字节操作数。
gdt_48:
	.word	0x800		! gdt limit=2048, 256 GDT entries 	
						; 表限长2k
	.word	512+gdt,0x9	! gdt base = 0X9xxxx 
						; （线性地址空间）基地址：0x90200 + gdt

;;;;; 以下跟显示卡相关（可以不看） ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
msg1:		.ascii	"Press <RETURN> to see SVGA-modes available or any other key to continue."
		db	0x0d, 0x0a, 0x0a, 0x00
msg2:		.ascii	"Mode:  COLSxROWS:"
		db	0x0d, 0x0a, 0x0a, 0x00
msg3:		.ascii	"Choose mode by pressing the corresponding number."
		db	0x0d, 0x0a, 0x00
		
idati:		.ascii	"761295520"
idcandt:	.byte	0xa5
idgenoa:	.byte	0x77, 0x00, 0x66, 0x99
idparadise:	.ascii	"VGA="

! Manufacturer:	  Numofmodes:	Mode:
; 厂家：			模式数量：		模式列表：
moati:		.byte	0x02,	0x23, 0x33 
moahead:	.byte	0x05,	0x22, 0x23, 0x24, 0x2f, 0x34
mocandt:	.byte	0x02,	0x60, 0x61
mocirrus:	.byte	0x04,	0x1f, 0x20, 0x22, 0x31
moeverex:	.byte	0x0a,	0x03, 0x04, 0x07, 0x08, 0x0a, 0x0b, 0x16, 0x18, 0x21, 0x40
mogenoa:	.byte	0x0a,	0x58, 0x5a, 0x60, 0x61, 0x62, 0x63, 0x64, 0x72, 0x74, 0x78
moparadise:	.byte	0x02,	0x55, 0x54
motrident:	.byte	0x07,	0x50, 0x51, 0x52, 0x57, 0x58, 0x59, 0x5a
motseng:	.byte	0x05,	0x26, 0x2a, 0x23, 0x24, 0x22
movideo7:	.byte	0x06,	0x40, 0x43, 0x44, 0x41, 0x42, 0x45

!			msb = Cols lsb = Rows:
;			高字节 = 列数 低字节 = 行数
dscati:		.word	0x8419, 0x842c
dscahead:	.word	0x842c, 0x8419, 0x841c, 0xa032, 0x5042
dsccandt:	.word	0x8419, 0x8432
dsccirrus:	.word	0x8419, 0x842c, 0x841e, 0x6425
dsceverex:	.word	0x5022, 0x503c, 0x642b, 0x644b, 0x8419, 0x842c, 0x501e, 0x641b, 0xa040, 0x841e
dscgenoa:	.word	0x5020, 0x642a, 0x8419, 0x841d, 0x8420, 0x842c, 0x843c, 0x503c, 0x5042, 0x644b
dscparadise:	.word	0x8419, 0x842b
dsctrident:	.word 	0x501e, 0x502b, 0x503c, 0x8419, 0x841e, 0x842b, 0x843c
dsctseng:	.word	0x503c, 0x6428, 0x8419, 0x841c, 0x842c
dscvideo7:	.word	0x502b, 0x503c, 0x643c, 0x8419, 0x842c, 0x841c
;;;;;; 显示卡相关 到这结束 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.text
endtext:
.data
enddata:
.bss
endbss:
