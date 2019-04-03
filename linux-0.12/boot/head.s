/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
/*
 * head.s含有32位启动代码。
 *
 * 注意!!! 32位启动代码是从绝对地址0x00000000开始的,这里也同样是页目录将存在的地方,因此这里的启动代
 * 码将被页目录覆盖掉.
 *
 */
.text
.globl idt,gdt,pg_dir,tmp_floppy_area
pg_dir: # 页目录将会存放在这里.
# 再次注意!!!这里已经处于32位运行模式,因此这里的$0x10并不是把地址0x10装入各个段寄存器,它现在其实是全
# 局段描述符表中的偏移值,或者更准确地说是一个描述符表项的选择符.这里$0x10的含义是请求特权级0(位0-1=0),
# 选择全局描述符表(位2=0),选择表中第2项(位3-15=2).它正好指向表中的数据段描述符项。
# 下面的代码含义是:设置ds,es,fs,gs为setup.s中构造的数据段(全局段描述符表第2项)的选择符=0x10,并将堆
# 栈放置在stack_start指向的user_stack数据区,然后使用本程序后面定义的新中断描述符表和全局段描述符表。
# 新全局段描述表中初始内容与setup.s中的基本一样,仅段限长从8MB修改成了16MB.
# stack_start定义在kernel/sched.c中.它是指向user_stack数组末端的一个长指针.下面设置这里使用的栈,
# 姑且称为系统栈.但在移动到任务0执行(init/main.c中)以后该栈就被用作任务0和任务1共同使用的用户栈了.
.globl startup_32
startup_32:
    # 对于GNU汇编, 每个直接操作数要以'$'开始, 否则表示地址。
    # 每个寄存器名都要以'$'开头,eax表示是32位的ax寄存器。
    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    lss stack_start, %esp   # 表示stack_start->ss:esp,设置系统堆栈.stack_start定义在
                            # kernel/sched.c中.
    call setup_idt          # 调用设置中断描述符表子程序.
    call setup_gdt          # 调用设置全局描述符表子程序.
    movl $0x10, %eax        # reload all the segment registers
    mov %ax, %ds            # after changing gdt. CS was already
    mov %ax, %es            # reloaded in 'setup_gdt'
    mov %ax, %fs            # 因为修改了gdt,所以需要重新装载所有的段寄存器.CS代码段寄存器已
                            # 经在setup_gdt中重新加载过了.
    mov %ax, %gs

    # 由于段描述符中的段限长从setup.s中的8MB改成了本程序设置的16MB,因此这里再次对所有段寄存器执行加
    # 载操作是必须的.另外,通过使用bochs跟踪观察,如果不对CS再次执行加载,那么在执行到
    # movl $0x10,%eax时CS代码段不可见部分中的限长还是8MB.这样看来应该重新加载CS.但是由于setup.s中
    # 的内核代码段描述符与本程序中重新设置的代码段描述符除了段限长以外其余部分完全一样,8MB的限长在内核
    # 初始化阶段不会有问题,而且在以后内核执行过程中段间跳转时会重新加载CS.因此这里没有加载它并没有让程
    # 序出错.针对该问题,目前内核中就在call setup_gdt之后添加了一条长跳转指
    # 令:'ljmp $(__KERNEL_CS),$1f',跳转到movl $0x10,$eax来确保CS确实被重新加载.

    lss stack_start, %esp

    # 下面代码用于测试A20地址线是否已经开启.采用的方法是向内存地址0x000000处写入任意一个数值,然后看
    # 内存地址0x100000(1M)处是否也是这个数值.如果一直相同的话,就一直比较下去,即死循环,死机.表示地
    # 址A20线没有选通,结果内核就不能使用1MB以上内存.
    xorl %eax, %eax
1:	incl %eax               # check that A20 really IS enabled
    # '1:'是一个局部符号构成的标号.标号由符号后跟一个冒号组成.此时该符号表示活动位置计数的当前值,并
    # 可以作为指令的操作数.局部符号用于帮助编译器和编程人员临时使用一些名称.共有10个局部符号名,可在
    # 整个程序中重复使用.这些符号名使用名称'0','1',...,'9'来引用.为了定义一个局部符号,需把标号写
    # 成'N:'形式(其中N表示一个数字).为了引用先前最近定义的这个符号,需要写成'Nb',其中N是定义标号时
    # 使用的数字.为了引用一个局部标号的下一个定义,而要与成'Nf',这里N是10个前向引用之一.上面'b'表
    # 示"向后(backwards)",'f'表示"向前(forwards)".在汇编程序的某一处,我们最大可以向后/向前引
    # 用10个标号.
    movl %eax, 0x000000     # loop forever if it isn't
    cmpl %eax, 0x100000
    je 1b                   # '1b'表示向后跳转到标号1去.若是'5f'则表示向前跳转到标号5去.
/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
/*
 * 注意!在下面这段程序中,486应该将位16置位,以检查在超级用户模式下的写保护,此后"verify_area()"调用就
 * 不需要了.486的用户通常也会想将NE(#5)置位,以便对数学协处理器的出错使用int 16.
 *
 */
 # 上面原注释中提到的486CPU中CR0控制器的位16是写保护标志WP,用于禁止超级用户级的程序向一般用户只读页面中
 # 进行写操作.该标志主要用于操作系统在创建新进程时实现写时复制方法.
 # 下面这段程序用于检查数学协处理器芯片是否存在.方法是修改控制寄存器CR0,在假设存在协处理器的情况下执行一个
 # 协处理器指令,如果出错的话则说明协处理器芯片不存在,需要设置CR0中的协处理器仿真位EM(位2),并复位协处理器
 # 存在标志MP(位1).

    movl %cr0, %eax						# check math chip
    andl $0x80000011, %eax				# Save PG,PE,ET
    /* "orl $0x10020,%eax" here for 486 might be good */
    orl $2, %eax						# set MP
    movl %eax, %cr0
    call check_x87
    jmp after_page_tables

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
/*
 * 我们依赖于ET标志的正确性来检测287/387存在与否.
 *
 */
# 下面fninit和fstsw是数学协处理器(80287/80387)的指令.
# fninit向协处理器发出初始化命令,它会把协处理器置于一个末受以前操作影响的已和状态,设置其控制字为默认值，
# 清除状态字和所有浮点栈式寄存器。非等待形式的这条指令(fninit)还会让协处理器终止执行当前正在执行的任何
# 先前的算术操作。fstsw指令取协处理器的状态字。如果系统中存在协处理器的话,那么在执行了fninit指令后其状
# 态字低字节肯定为0.

check_x87:
    fninit								# 向协处理器发出初始化命令.
    fstsw %ax							# 取协处理器状态字到ax寄存器中.
    cmpb $0, %al						# 初始化状态字应该为0,否则说明协处理器不存在.
    je 1f								/* no coprocessor: have to set bits */
    movl %cr0, %eax						# 如果存在则向前跳转到标号1处,否则改写cr0.
    xorl $6, %eax						/* reset MP, set EM */
    movl %eax, %cr0
    ret

# 下面是一个汇编语言指示符.其含义是指存储边界对齐调整."2"表示把随后的代码或数据的偏移位置调整到地址值
# 最后2位为零的位置(2^2)，即按4字节方式对齐内存地址.不过现在GNU as直接写出对齐的值而非2的次方值了。
# 使用该指示符的目的是为了提高32位CPU访问内存中代码或数据的速度和效率。下面的两个字节值是80287协处理
# 器指令fsetpm的机器码。其作用是把80287设置为保护模式。
# 80387无需该指令,并且将会把该指令看作是空操作.

.align 2
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */	# 287协处理器码.
    ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
/*
 * 下面这段是设置中断描述符表子程序setup_idt
 *
 * 将中断描述符表idt设置成具有256个项,并都指向ignore_int中断门.然后加载中断描述符表寄存器(lidt指令)。
 * 真正实用的中断门以后再安装.当我们在其他地方认为一切都正常时再开启中断.该子程序将会被页表覆盖掉.
 */
# 中断描述符表中的项虽然也是8字节组成,但其格式与全局表中的不同,被称为门描述符.它的0-1,6-7字节是偏移量，
# 2-3字节是选择符,4-5字节是一些标志.该描述符,共256项.eax含有描述符低4字节,edx含有高4字节.内核在随后
# 的初始化过程中会替换安装那些真正实用的中断描述符项。

setup_idt:
    lea ignore_int, %edx		# 将ignore_int的有效地址(偏移值)值->eax寄存器
    movl $0x00080000, %eax		# 将选择符0x0008置入eax的高16位中.
    movw %dx, %ax				/* selector = 0x0008 = cs */			
                                # 偏移值的低16位置入eax的低16位中.此时eax含有门描述符低4字节的值。
    movw $0x8E00, %dx			/* interrupt gate - dpl=0, present */	
                                # 此时edx含有门描述符高4字节的值.

    lea idt, %edi				# idt是中断描述符表的地址.
    mov $256, %ecx
rp_sidt:
    movl %eax, (%edi)			# 将哑中断门描述符存入表中.
    movl %edx, 4(%edi)
    addl $8, %edi				# edi指向表中下一项.
    dec %ecx
    jne rp_sidt
    lidt idt_descr				# 加载中断描述符表寄存器值.
    ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
/*
 * 设置全局描述符表项setup_gdt
 * 这个子程序设置一个新的全局描述符表gdt,并加载.该子程序将被页表覆盖掉.
 *
 */
setup_gdt:
    lgdt gdt_descr	# 加载全局描述符表寄存器(内容已设置好)
    ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
/*
 * Linus将内核的内存页表直接放在页目录之后,使用了4个表来寻址16MB的物理内存.如果你有多于16MB的内存，
 * 就需要在这里进行扩充修改.
 *
 */
 # 每个页表长为4KB字节(1页内存页面),而每个页表项需要4个字节,因此一个页表共可以存放1024个表项.如果一
 # 个页表项寻址4KB的地址空间,则一个页表就可以寻址4MB的物理内存.
 # 页表项的格式为:项的前0-11位存放一些标志,例如是否在内存中(P位0),读写许可(R/W位1),普通还是超级用户
 # 使用(U/S位2),是否修改过了(是否脏了)(D位6)等;
 # 表项的位12-31是页框地址,用于指出一页内存的物理起始地址.

.org 0x1000					# 从偏移0x1000处开始的第1个页表(偏移0开始处将存放页表目录).
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000					# 定义下面的内存数据块从偏移0x5000处开始.
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
/*
 * 当DMA(直接存储器访问)不能访问缓冲块时,下面的tmp_floppy_area内存块就可供软盘驱动程序使用.其地址
 * 需要对齐调整,这样就不会跨越64KB边界.
 */

tmp_floppy_area:
    .fill 1024,1,0			# 共保留1024项,每项1B,填充数值0.

 # 下面这几个入栈操作用于为跳转到init/main.c中的main()函数作准备工作.pushl $L6指令在栈中压入返回
 # 地址,而pushl $main则压入了main()函数代码的地址.当head.s最后执行ret指令时就会弹出main()的地址,
 # 并把控制权转移到init/main.c程序中.
 # 前面3个入栈0值应该分别表示envp,argv指针和argc的值,但main()没有用到.

after_page_tables:
    pushl $0						# These are the parameters to main :-)
    pushl $0						# 这些是调用main程序的参数(指init/main.c).
    pushl $0						# 其中的'$'符号表示这是一个立即操作数.
    pushl $L6						# return address for main, if it decides to.
    pushl $main						# 'main'是编译程序对main的内部表示方法.
    jmp setup_paging				# 跳转至setup_paging
L6:
    jmp L6							# main should never return here, but
                                    # just in case, we know what happens.
                                    # main程序绝对不应该返回到这里.不过为了以防万一,所以添加了
                                    # 该语句.这样我们就知道发生什么问题了.

/* This is the default interrupt "handler" :-) */
/* 下面是默认的中断"向量句柄" */

int_msg:
    .asciz "Unknown interrupt\n\r"	# 定义字符串"末知中断(回车换行)".

.align 2							# 按4字节方式对齐内存地址.
ignore_int:
    pushl %eax
    pushl %ecx
    pushl %edx
    push %ds						# 这里请注意!!ds,es,fs,gs等虽然是16位寄存器,但入栈后仍然
                                    # 会以32位的形式入栈,即需要占用4个字节的堆栈空间.
    push %es
    push %fs
    movl $0x10, %eax				# 设置段选择符(使ds,es,fs指向gdt表中的数据段).
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    pushl $int_msg					# 把调用printk函数的参数指针(地址)入栈.注意!若int_msg前不
                                    # 加'$',则表示把int_msg符处的长字('Unkn')入栈.
    call printk						# 该函数在/kernel/printk.c中.
    popl %eax
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    iret							# 中断返回(把中断调用时压入栈的CPU标志寄存器(32位)值也弹出).


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
/*
 * 这个子程序通过设置控制寄存器cr0的标志(PG位31)来启动对内存的分页处理功能,并设置各个页表项的内容,以恒
 * 等映射前16MB的物理内存.分页器假定不会产生非法的地址映射(也即在只有4MB的机器上设置出大于4MB的内存地
 * 址).
 *
 * 注意!尽管所有的物理地址都应该由这个子程序进行恒等映射,但只有内核页面管理函数能直接使用>1MB的地址.所
 * 有"普通"函数仅使用低于1MB的地址空间,或者是使用局部数据空间,该地址空间将被映射到其他一些地方去
 * --mm(内存管理程序)会管理这些事的.
 *
 */
 # 上面英文注释第2段的含义是指在机器物理内存中大于1MB的内存空间主要被用于主内存区.主内存区空间由mm模块
 # 管理.它涉及页面映射操作.内核中所有其它函数就是这里指的一般(普通)函数.若要使用主内存区的页面,就需要使
 # 用get_free_page()等函数获取.因为主内存区中内存页面是共享资源,必须有进行统一管理以避免资源争用和竞争.
 #
 # 在内存物理地址0x0处开始存放1页页目录表和4页页表.页目录表是系统所有进程共用的,而这里的4页页表则属于内
 # 核专用,它们一一映射线性地址起始16MB空间范围到物理内存上.对于新的进程,系统会在主内存区为其申请页面存
 # 放页表.另外,1页内存长度是4096字节.

.align 2								# 按4字节方式对齐内存地址边界.
setup_paging:							# 首先对5页内存(1页目录+4页页表)清零.
    movl $1024 * 5, %ecx				/* 5 pages - pg_dir+4 page tables */
    xorl %eax, %eax
    xorl %edi, %edi						/* pg_dir is at 0x000 */	
                                        # 页目录从0x0000地址开始
    cld;rep;stosl						# eax内容存到es:edi所指内存位置处,且edi增4.

     # 下面4句设置页目录表中的项,因为我们(内核)共有4个页表所以只需设置4项.
     # 页目录项的结构与页表项的结构一样,4个字节为1项.
     # 例如"$pg0+7"表示:0x00001007,是页目录表中的第1项.
     # 则第1个页表所在的地址=0x00001007 & 0xfffff000=0x1000;
     # 第1个页表的属性标志=0x00001007 & 0x00000fff = 0x07,表示该页存在,用户可读写.
    movl $pg0 + 7, pg_dir				/* set present bit/user r/w */
    movl $pg1 + 7, pg_dir + 4			/*  --------- " " --------- */
    movl $pg2 + 7, pg_dir + 8			/*  --------- " " --------- */
    movl $pg3 + 7, pg_dir + 12			/*  --------- " " --------- */

     # 下面6行填写4个页表中所有项的内容,共有:4(页表)*1024(项/页表)=4096项(0-0xfff),即能映射物理
     # 内存4096*4KB = 16MB.
     # 每项的内容是:当前项所映射的物理内存地址 + 该页的标志(这里均为7).
     # 使用的方法是从最后一个页表的最后一项开始按倒退顺序填写.一个页表的最后一项在页表中的位置
     # 是1023*4 = 4092.因此最后一页的最后一项的位置就是$pg3+4092.

    movl $pg3 + 4092, %edi			# edi->最后一页的最后一项.
    movl $0xfff007, %eax			/*  16Mb - 4096 + 7 (r/w user,p) */
                                    # 最后一项对应物理内存页的地址是0xfff000,加上属性
                                    # 标志7,即为xfff007.
    std								# 方向位置位,edi值递减(4字节).
1:	stosl							/* fill pages backwards - more efficient :-) */
    subl $0x1000, %eax				# 每填好一项,物理地址值减0x1000.
    jge 1b							# 如果小于0则说明全填写好了.
    cld
     # 设置页目录表基地址寄存器cr3的值,指向页目录表.cr3中保存的是页目录表的物理地址.
    xorl %eax, %eax						/* pg_dir is at 0x0000 */		
                                        # 页目录表在0x0000处.
    movl %eax, %cr3						/* cr3 - page directory start */
    # 设置启动使用分页处理(cr0的PG标志,位31)
    movl %cr0, %eax
    orl $0x80000000, %eax			# 添上PG标志.
    movl %eax, %cr0					/* set paging (PG) bit */
    ret								/* this also flushes prefetch-queue */

# 在改变分页处理标志后要求使用转移指令刷新预取指令队列, 这里用的是返回指令ret.
# 该返回指令的另一个作用是将pushl $main压入堆栈中的main程序的地址弹出,并跳转到/init/main.c程序去运
# 行.本程序到此就真正结束了.

.align 2							# 按4字节方式对齐内存地址边界.
.word 0								# 这里先空出2字节,这样.long _idt的长字是4字节对齐的.

# 下面是加载中断描述符表寄存器idtr的指令lidt要求的6字节操作数.前2字节是idt表的限长,后4字节是idt表在
# 线性地址空间中的32位基地址.
idt_descr:
    .word 256 * 8 - 1				# idt contains 256 entries
    .long idt
.align 2
.word 0

# 下面是加载全局描述符表寄存器gdtr的指令lgdt要求的6字节操作数.前2字节是gdt表的限长,后4字节是gdt表的
# 线性基地址.这里全局表长度设置为2KB字节(0x7ff即可),因为每8字节组成一个描述符项,所以表中共可有256项。
# 符号gdt是全局表在本程序中的偏移位置.

gdt_descr:
    .word 256 * 8 - 1					# so does gdt (not that that's any
    .long gdt							# magic number, but it works for me :^)

    .align 8							# 按8(2^3)字节方式对齐内存地址边界.
idt:	.fill 256, 8, 0					# idt is uninitialized	# 256项,每项8字节,填0.

# 全局表,前4项分别是空项(不用),代码段描述符,数据段描述符,系统调用段描述符,其中系统调用段描述符并没有
# 派用处,Linus当时可能曾想把系统调用代码专门放在这个独立的段中。同时还预留了252项的空间,用于放置所创
# 建任务的局部描述符(LDT)和对应的任务状态段TSS的描述符.
# (0-nul, 1-cs, 2-ds, 3-syscall, 4-TSS0, 5-LDT0, 6-TSS1, 7-LDT1, 8-TSS2 etc...)
gdt:
    .quad 0x0000000000000000			/* NULL descriptor */
    .quad 0x00c09a0000000fff			/* 16Mb */		# 0x08,内核代码段最大长度16MB.
    .quad 0x00c0920000000fff			/* 16Mb */		# 0x10,内核数据段最大长度16MB.
    .quad 0x0000000000000000			/* TEMPORARY - don't use */
    .fill 252, 8, 0						/* space for LDT's and TSS's etc */	# 预留空间.
