! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
; SYS_SIZE是要加载的系统模块长度的节数（每节有16b）。0x3000节就等
; 于0x30000bytes=192KB，对于当前的版本空间已足够了。

; 该头文件里定义了内核用到的一些常数符号和Linus自己使用的默认硬盘默认参数块。
#include <linux/config.h>

SYSSIZE = DEF_SYSSIZE 	; 系统模块大小为0x3000节

!
!	bootsect.s (C) 1991 Linus Torvalds
!	modified by Drew Eckhardt
!
! bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
! iself out of the way to address 0x90000, and jumps there.
!
! It then loads 'setup' directly after itself (0x90200), and the system
! at 0x10000, using BIOS interrupts.
!
! NOTE! currently system is at most 8*65536 bytes long. This should be no
! problem, even in the future. I want to keep it simple. This 512 kB
! kernel size should be enough, especially as this doesn't contain the
! buffer cache as in minix
!
! The loader has been made as simple as possible, and continuos
! read errors will result in a unbreakable loop. Reboot by hand. It
! loads pretty fast by getting whole sectors at a time whenever possible.

; 以下是前面这些文字的翻译：
; 	bootsect.s (C) 1991 Linus Torvalds 版权所有
;	Drew Eckhardt 修改
;
; bootsect.s被BIOS启动子程序加载至0x7c00处，并将自己移到了地址0x90000处，并跳转至那里。
;
; 它然后使用BIOS中断将setup直接加载到自己的后面(0x90200)，并将system加载到地址0x10000处。
;
; 注意! 目前的内核系统最大长度限制为512KB字节，即使是在将来这也应该没有问题的。我想让它保持简
; 单明了。这样512KB的最大内核长度应该足够了，尤其是这里没有像MINIX中一样包含缓冲区。
;
; 加载程序已经做得够简单了，而且持续的读操作出错将导致死循环。就只能手工重启。
; 只要能够一次读取所有的扇区，加载过程可以做的很快的。

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4					! nr of setup-sectors
                                ; setup 占用的磁盘扇区数
BOOTSEG  = 0x07c0				! original address of boot-sector
                                ; bootsect 代码所在的原地址（被BIOS子程序加载至此处）
INITSEG  = DEF_INITSEG			! we move boot here - out of the way
                                ; bootsect将要移动到的目的段位置，为了避开系统模块占用处
SETUPSEG = DEF_SETUPSEG			! setup starts here
                                ; setup程序代码的段位置
SYSSEG   = DEF_SYSSEG			! system loaded at 0x10000 (65536).
                                ; system模块将被加载到0x10000
ENDSEG   = SYSSEG + SYSSIZE		! where to stop loading
                                ; 停止加载的段地址

! ROOT_DEV & SWAP_DEV are now written by "build".
; 根文件系统设备号ROOT_DEV和交换设备号SWAP_DEV现在由tools目录下的build程序写入。

ROOT_DEV = 0
SWAP_DEV = 0

entry start 	; 告知链接程序，程序从start标号处开始执行
start:
;;;;; 1. 将自身(bootsect)从0x7c00移动到0x90000处，共256字(512字节) ;;;;;;;;;;;;;;;;;;
    mov	ax,#BOOTSEG
    mov	ds,ax
    mov	ax,#INITSEG
    mov	es,ax
    mov	cx,#256
    sub	si,si       ; 源地址 ds:si = 0x07c0:0x0000
    sub	di,di       ; 目标地址 es:di = 0x9000:0x0000
    rep
    movw            ; 此处结束后，代码已经成功移动到0x90000
    jmpi go,INITSEG ; 段间跳转(Jump Intersegment)，跳转到INITSEG:go(段地址:段内偏移)处。

;;;;; 从go处开始，CPU在已移动到0x90000处继续执行 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 以下几行代码设置了几个段寄存器，包括栈寄存器ss和sp。
go:	mov	ax,cs
    mov	dx,#0xfef4	! arbitrary value >>512 - disk parm size
                    ; 栈指针要远大于512字节偏移(即0x90200)处都可以；一般setup程序大概占用4个
                    ; 扇区，所以sp要大于(0x90200+0x200*4+堆栈大小)。
                    ; 这里sp被设置成了0x9ff00-12(参数表长度)，即sp = 0xfef4。
    mov	ds,ax
    mov	es,ax
    ;push   ax		; 临时保存段值(0x9000)，供后面使用。

    mov	ss,ax		! put stack at 0x9ff00 - 12.
    mov	sp,dx
/*
 *	Many BIOS's default disk parameter tables will not
 *	recognize multi-sector reads beyond the maximum sector number
 *	specified in the default diskette parameter tables - this may
 *	mean 7 sectors in some cases.
 *
 *	Since single sector reads are slow and out of the question,
 *	we must take care of this by creating new parameter tables
 *	(for the first disk) in RAM.  We will set the maximum sector
 *	count to 18 - the most we will encounter on an HD 1.44.
 *
 *	High doesn't hurt.  Low does.
 *
 *	Segments are as follows: ds=es=ss=cs - INITSEG,
 *		fs = 0, gs = parameter table segment
 */
/*
 * 对于多扇区读操作所读的扇区数超过默认磁盘参数表中指定的最大扇区数时，很多BIOS
 * 将不能进行正确识别。在某些情况下是7个扇区。
 *
 * 由于单扇区读操作太慢，不予考虑。我们必须通过在内存中重新创建新的参数表(为第1个驱动器)
 * 来解决这个问题。我们将把其中最大扇区数设置为18，即在1.44MB磁盘上会碰到的最大值。
 *
 * 数值大不会出问题，但太小就不行了。
 *
 * 段寄存器将被设置成：ds = es = ss = cs 都为INITSEG(0x9000),
 * fs = 0, gs = 参数表所在段值。
 */

;;;;; 修改软驱参数表 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; BIOS中断0x1e的中断向量值是软驱参数表地址。该向量值位于内存0x1e*4=0x78处。这段代码首先从内
; 存0x0000:0x0078处复制原软驱参数表到0x9000:0xfef4处，然后修改表中的每磁道最大扇区数为18。

    push	#0
    pop		fs
    mov	bx,	#0x78		; fs:bx is parameter table address

    seg fs              ; seg fs只影响接下来的一条语句，表示下一条语句的操作数在fs所指的段中
    lgs	si,(bx)			! gs:si is source       ; 将fs:bx赋值给gs:si 0x0000:0x0078
    mov	di,dx			! es:di is destination  ;                   0x9000:0xfef4
    mov	cx,#6			! copy 12 bytes
    cld

    rep
    seg gs
    movw

    mov	di,dx
    movb 4(di),*18		! patch sector count ;修改新表的最大扇区数为18

    seg fs 				; 让中断向量0x1e的值指向新表
    mov	(bx),di
    seg fs
    mov	2(bx),es

    mov ax,cs           ; pop ax	! ax = 0x9000
    mov	fs,ax
    mov	gs,ax

    xor	ah,ah			! reset FDC ; 复位软盘控制器，让其采用新参数。
    xor	dl,dl 			! dl = 0    ; 第1个软驱
    int 0x13

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.
; 在bootsect程序块后紧跟着加载setup模块的代码数据。
; 在移动代码时，es的值已被设置好。

;;;;; 2. 加载setup模块到0x90200开始处，共读4个扇区 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 如果读出错，显示磁盘上出错扇区位置，则复位驱动器，并重试，没有退路。
load_setup:
    xor	dx, dx					! drive 0, head 0
    mov	cx,#0x0002				! sector 2, track 0
    mov	bx,#0x0200				! address = 512, in INITSEG
    mov	ax,#0x0200+SETUPLEN		! service 2, nr of sectors
    int	0x13					! read it
    jnc	ok_load_setup 			! ok - continue ;jnc - jump not cf

    push	ax			! dump error code
    call	print_nl
    mov	bp, sp
    call	print_hex
    pop	ax

    xor	dl, dl			! reset FDC ;复位磁盘控制器，重试。
    xor	ah, ah
    int	0x13
    j	load_setup 		; j - jmp

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track
; 这段代码取磁盘驱动器的参数，特别是每道的扇区数量，并保存在位置sectors处。
    xor	dl,dl
    mov	ah,#0x08        ! AH=8 is get drive parameters
    int	0x13
    xor	ch,ch
    seg cs
    mov	sectors,cx      ; 保存每磁道扇区数。
    mov	ax,#INITSEG
    mov	es,ax           ; 上面取磁盘参数中断改了es的值，这里需要改回来

! Print some inane message
; 在显示一些信息('Loading\r\n'，共9个字符)。
    mov	ah,#0x03        ! read cursor pos
    xor	bh,bh
    int	0x10

    mov	cx,#9
    mov	bx,#0x0007      ! page 0, attribute 7 (normal)
    mov	bp,#msg1
    mov	ax,#0x1301      ! write string, move cursor
    int	0x10
! ok, we've written the message, now

! we want to load the system (at 0x10000)
;;;;; 3. 将system模块加载到0x10000(64K)处 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
    mov	ax,#SYSSEG
    mov	es,ax			! segment of 0x010000
    call	read_it 	; 读磁盘上system模块
    call	kill_motor 	; 关闭驱动器马达
    call	print_nl

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.
; 此后，我们检查要使用哪个根文件系统设备。如果已经指定了设备(!=0)就直接使用给定
; 的设备。否则就需要根据BIOS报告的每磁道扇区数来确定到底使用/dev/PS0(2,28)
; 还是/dev/at0(2,8)。
; 在Linux中，软驱的主设备号是2，次设备号 = type << 2 + nr，
;       type    软驱的类型（2->1.2M或7->1.44M等）。
;       nr      0-3分别对应软驱A、B、C或D；

    seg cs
    mov	ax,root_dev
    or	ax,ax
    jne	root_defined
    seg cs              ; 取出sectors的值(每磁道扇区数)
    mov	bx,sectors      
    mov	ax,#0x0208      ! /dev/PS0 - 1.2Mb
    cmp	bx,#15          ; sectors=15则说明是1.2MB的软驱
    je	root_defined
    mov	ax,#0x021c      ! /dev/PS0 - 1.44Mb
    cmp	bx,#18          ; sectors=18则说明是1.44MB的软驱
    je	root_defined
undef_root:
    jmp undef_root
root_defined:
    seg cs
    mov	root_dev,ax

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:

;;;;; 4. 到此，所有程序都加载完毕，我们就跳转到setup程序去 ;;;;;;;;;;;;;;;;;;;;;;;;;
    jmpi	0,SETUPSEG

;;;;; bootsect.S程序到此就结束了 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 下面是几个子程序:
; read_it 用于读取磁盘上的system模块
; kill_motor 用于关闭软驱电动机
; 还有一些屏幕显示子程序

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
; 该子程序将系统模块system加载到内存地址0x10000处，并确定没有跨越64KB的内存边界。
sread:	.word 1+SETUPLEN	! sectors read of current track
                            ; 当前磁道的已读扇区数（bootsect + setup）
head:	.word 0				! current head  ;当前磁头号
track:	.word 0				! current track ;当前磁道号

read_it:
; 首先测试输入的段值。必须位于内存地址64KB边界处，否则进入死循环。
    mov ax,es
    test ax,#0x0fff
die:
    jne die			! es must be at 64kB boundary   ; es值必须位于64KB地址边界。
    xor bx,bx		! bx is starting address within segment
rp_read:
    mov ax,es
    cmp ax,#ENDSEG		! have we loaded all yet? ; 是否已经加载了全部数据？
    jb ok1_read
    ret
ok1_read:
    ; 计算和验证当前磁道需要读取的扇区数，放在ax寄存器中。
    seg cs
    mov ax,sectors
    sub ax,sread
    mov cx,ax
    shl cx,#9
    add cx,bx
    jnc ok2_read
    je ok2_read
    xor ax,ax
    sub ax,bx
    shr ax,#9
ok2_read:
    call read_track ; 读当前磁道上指定扇区和需读扇区数的数据
    mov cx,ax
    add ax,sread
    seg cs
    cmp ax,sectors
    jne ok3_read
    mov ax,#1
    sub ax,head
    jne ok4_read
    inc track
ok4_read:
    mov head,ax
    xor ax,ax
ok3_read:
    mov sread,ax
    shl cx,#9
    add bx,cx
    jnc rp_read
    mov ax,es
    add ah,#0x10
    mov es,ax
    xor bx,bx
    jmp rp_read

; 读当前磁道上指定开始扇区和需读扇区数的数据到es:bx开始处。
read_track:
    pusha			; push all
    pusha
    mov	ax, #0xe2e 	! loading... message 2e = .
    mov	bx, #7
    int	0x10
    popa
    ; 开始进行磁道扇区读操作
    mov dx,track
    mov cx,sread
    inc cx
    mov ch,dl
    mov dx,head
    mov dh,dl
    and dx,#0x0100 
    mov ah,#2

    push	dx      ! save for error dump
    push	cx      ; 保留出错情况
    push	bx
    push	ax

    int 0x13
    jc bad_rt
    add	sp, #8      ; 若没有出错，丢弃为出错情况保存的信息
    popa
    ret

; 读磁盘操作出错
bad_rt:
    push	ax          ! save error code
    call	print_all   ! ah = error, al = read

    xor ah,ah
    xor dl,dl
    int 0x13

    add	sp, #10
    popa
    jmp read_track

/*
 *	print_all is for debugging purposes.
 *	It will print out all of the registers.  The assumption is that this is
 *	called from a routine, with a stack frame like
 *	dx
 *	cx
 *	bx
 *	ax
 *	error
 *	ret <- sp
 *
*/
; print_all 用于调试目的，前提是从一个子程序中调用。栈帧结构如上所示
print_all:
    mov	cx, #5          ! error code + 4 registers
    mov	bp, sp          ; 保存当前栈指针sp.

print_loop:
    push	cx          ! save count left
    call	print_nl    ! nl for readability
    jae		no_reg      ! see if register name is needed

    ; 下面几行用于显示寄存器号，例如："AX:", 0x45 - E
    mov	ax, #0xe05 + 0x41 - 1 ; ah = 功能号(0x0e)；al = 字符(0x05 + 0x41 - 1)
    sub	al, cl
    int	0x10

    mov	al, #0x58       ! X
    int	0x10

    mov	al, #0x3a       ! :
    int	0x10
; 显示寄存器bp所指栈中的内容
no_reg:
    add	bp, #2          ! next register
    call	print_hex   ! print it
    pop	cx
    loop	print_loop
    ret

; 显示回车换行
print_nl:
    mov	ax, #0xe0d      ! CR
    int		0x10
    mov	al, #0xa        ! LF
    int 	0x10
    ret

/*
 *	print_hex is for debugging purposes, and prints the word
 *	pointed to by ss:bp in hexadecmial.
*/
/* 子程序print_hex用于调试目的.它使用十六进制在屏幕上显示出ss:bp指向的字 */
print_hex:
    mov	cx, #4      ! 4 hex digits
    mov	dx, (bp)    ! load word into dx

; 先显示高字节，因此需要把dx中值左旋4位，此时高4位在dx的低4位中
print_digit:
    rol	dx, #4      ! rotate so that lowest 4 bits are used ;左旋4位
    mov	ah, #0xe
    mov	al, dl      ! mask off so we have only next nibble
    and	al, #0xf    ; 只取低四位显示
    add	al, #0x30   ! convert to 0 based digit, '0'
    cmp	al, #0x39   ! check for overflow ; 大于9的处理，转换成A-F
    jbe	good_digit
    add	al, #0x41 - 0x30 - 0xa  ! 'A' - '0' - 0xa

good_digit:
    int	0x10
    loop	print_digit
    ret

/*
 * This procedure turns off the floppy drive motor, so
 * that we enter the kernel in a known state, and
 * don't have to worry about it later.					'
 */
; 这个子程序用于关闭软驱的马达，这样我们进入内核后它处于已知状态，以后也就无须担心它了。
kill_motor:
    push dx
    mov dx,#0x3f2
    xor al, al
    outb
    pop dx
    ret

sectors:
    .word 0

msg1:
    .byte 13,10
    .ascii "Loading"

.org 506
; swap_dev在第506开始的2个字节中，root_dev在第508开始的2个字节中
swap_dev:
    .word SWAP_DEV
root_dev:
    .word ROOT_DEV

; 下面2个字节是有效引导扇区的标志（必须位于引导扇区的最后两个字节中）
boot_flag:
    .word 0xAA55

.text
endtext:
.data
enddata:
.bss
endbss:
