/*
 *  linux/fs/exec.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 */
 /*
 * #!开始脚本程序的检测代码部分是由tytso实现的.
 */

/*
 * Demand-loading implemented 01.12.91 - no need to read anything but
 * the header into memory. The inode of the executable is put into
 * "current->executable", and page faults do the actual loading. Clean.
 *
 * Once more I can proudly say that linux stood up to being changed: it
 * was less than 2 hours work to get demand-loading completely implemented.
 */
/*
 * 需求时加载实现于1991.12.1 - 只需将执行文件头部读进内存而无须将整个执行文件都加载进内存。执行
 * 文件的i节点被放在当前进程的可执行字段中"current->executable"，页异常会进行执行文件的实际加载
 * 操作。这很完美。
 *
 * 我可再一次自豪地说，linux经得起修改：只用了不到2小时的工作就完全实现了需求加载处理。
 */

#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 */
/*
 * MAX_ARG_PAGES定义了为新程序分配的给参数和环境变量使用的最大内存页数。32页内存应该足够了，这
 * 使得环境和参数(env+arg)空间的总和达到128KB!
 */
#define MAX_ARG_PAGES 32

/**
 * 使用库文件
 * 为进程选择一个库文件，并替换进程当前库文件i节点字段值为这里指定库文件名的i节点指针。如果
 * library指针为空，则把进程当前的库文件释放掉。
 * @param[in]       library     库文件名
 * @retval          成功返回0，失败返回出错码
 */
int sys_uselib(const char * library)
{
	struct m_inode * inode;
	unsigned long base;

	/* 根据进程的空间长度，判断是否为普通进程 */
	if (get_limit(0x17) != TASK_SIZE) {
		return -EINVAL;
	}
	if (library) {
		if (!(inode = namei(library))) {		/* get library inode */
			return -ENOENT;
		}
	} else {
		inode = NULL;
	}
/* we should check filetypes (headers etc), but we don't */
/* 我们应该检查一下文件类型（如头部信息等），但是我们还没有这样做。*/
	iput(current->library);
	current->library = NULL;
	base = get_base(current->ldt[2]);
	base += LIBRARY_OFFSET;
	free_page_tables(base, LIBRARY_SIZE);
	current->library = inode;
	return 0;
}

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 */
/*
 * create_tables()函数在新任务内存中解析环境变量和参数字符串，由此创建指针表，并将它们的地址放
 * 到"栈"上，然后返回新栈的指针值。
 */

/**
 * 在新任务中创建参数和环境变量指针表
 * @param[in]	p		数据段中参数和环境信息偏移指针
 * @param[in]	argc	参数个数
 * @param[in]	envc	环境变量个数
 * @retval		栈指针值
 */
static unsigned long * create_tables(char * p, int argc, int envc)
{
	unsigned long *argv, *envp;
	unsigned long * sp;

	/* 栈指针是以4字节为边界进行寻址的，故为0xfffffffc */
	sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
	/* 多留一个位置用于存放NULL */
	sp -= envc + 1;
	envp = sp;
	sp -= argc + 1;
	argv = sp;
	put_fs_long((unsigned long)envp, --sp);
	put_fs_long((unsigned long)argv, --sp);
	put_fs_long((unsigned long)argc, --sp);
	while (argc-- > 0) {
		put_fs_long((unsigned long) p, argv++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0, argv);
	while (envc-- > 0) {
		put_fs_long((unsigned long) p, envp++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0, envp);
	return sp;
}

/*
 * count() counts the number of arguments/envelopes
 */
/*
 * count()函数计算命令行参数/环境变更的个数
 */
 
/**
 * 统计参数指针数组中指针的个数
 * @param[in]	argv	参数指针数组(最后一个指针项是NULL)
 * @retval		参数个数
 */
static int count(char ** argv)
{
	int i = 0;
	char ** tmp;

	if ((tmp = argv)) {
		while (get_fs_long((unsigned long *) (tmp++))) {
			i++;
		}
	}

	return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 *
 * Modified by TYT, 11/24/91 to add the from_kmem argument, which specifies
 * whether the string and the string array are from user or kernel segments:
 * 
 * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 *    2          kernel space  kernel space
 * 
 * We do this by playing games with the fs segment register.  Since it
 * it is expensive to load a segment register, we try to avoid calling
 * set_fs() unless we absolutely have to.
 */
/*
 * 'copy_string()'函数从用户内存空间复制参数/环境字符串到内核空闲页面中。这些已具有直接放到新用
 * 户内存中的格式。
 *
 * 由TYT(Tytso)于1991.11.24日修改，增加了from_kmem参数，该参数指明了字符串或字符串数组是来自用
 * 户段还是内核段.
 *
 * from_kmem	 指针 argv *		字符串 argv **
 *	  0 		 用户空间			用户空间
 *	  1 		 内核空间			用户空间
 *	  2 		 内核空间			内核空间
 *
 * 我们是通过巧妙处理fs段寄存器来操作的。由于加载一个段寄存器代价太高，所以我们尽量避免调用
 * set_fs()，除非实在必要。
 */

/**
 * 复制指定个数的参数字符串到参数和环境空间中
 * 在do_execve()函数中，p初始化为指向参数表(128KB)空间的最后一个长字处，参数字符串是以堆栈操作
 * 方式逆向往其中复制存放的。因此p指针会随着复制信息的增加而逐渐减小，并始终指向参数字符串的头
 * 部。字符串来源标志from_kmem应该是TYT为了给execve()增添执行脚本文件的功能而新加的参数。当没
 * 有运行脚本文件的功能时，所有参数字符串都在用户数据空间中。
 * @param[in]	argc		欲添加的参数个数
 * @param[in]	argv		参数指针数组
 * @param[in]	page		参数和环境空间页面指针数组
 * @param[in]	p           参数表空间中偏移指针，始终指向已复制串的头部
 * @param[in]	from_kmem   字符串来源标志
 * @retval		成功返回参数和环境空间当前头部指针，出错返回0
 */
static unsigned long copy_strings(int argc, char ** argv, unsigned long *page,
		unsigned long p, int from_kmem)
{
	char *tmp, *pag;
	int len, offset = 0;
	unsigned long old_fs, new_fs;

	if (!p) {
		return 0;	/* bullet-proofing */
	}
	new_fs = get_ds();
	old_fs = get_fs();
	if (from_kmem == 2) {
		set_fs(new_fs);
	}
	/* 从最后一个参数逆向开始复制 */
	while (argc-- > 0) {
		if (from_kmem == 1) {
			set_fs(new_fs);
		}
		if (!(tmp = (char *)get_fs_long(((unsigned long *)argv) + argc))) {
			panic("argc is wrong");
		}
		if (from_kmem == 1) {
			set_fs(old_fs);
		}
		len = 0;		/* remember zero-padding */ /* 串是以NULL结尾的 */
		do {
			len++;
		} while (get_fs_byte(tmp++));
		/* 不会发生，参数表空间够大（128KB） */
		if (p - len < 0) {	/* this shouldn't happen - 128kB */
			set_fs(old_fs);
			return 0;
		}
		/* 把参数对应的字符串中的逐个字符（从尾到头）地复制到参数和环境空间末端处 */
		while (len) {
			--p; --tmp; --len;
			if (--offset < 0) {
				offset = p % PAGE_SIZE;
				if (from_kmem == 2) {
					set_fs(old_fs);
				}
				if (!(pag = (char *) page[p/PAGE_SIZE]) &&
				    !(pag = (char *) (page[p/PAGE_SIZE] = get_free_page()))) {
					return 0;
				}
				if (from_kmem == 2) {
					set_fs(new_fs);
				}
			}
			*(pag + offset) = get_fs_byte(tmp);
		}
	}
	if (from_kmem == 2) {
		set_fs(old_fs);
	}
	return p;
}


/**
 * 修改任务的局部描述符表内容
 * 修改局部描述符表LDT中描述符的段基址和段限长，并将参数和环境空间页面放置在数据段末端。
 * @param[in]	text_size	执行文件头部中a_text字段给出的代码段长度值
 * @param[in]	page		参数和环境空间页面指针数组
 * @retval		数据段限长值(64MB)
 */
static unsigned long change_ldt(unsigned long text_size, unsigned long * page)
{
	unsigned long code_limit, data_limit, code_base, data_base;
	int i;

	code_limit = TASK_SIZE;
	data_limit = TASK_SIZE;
	code_base = get_base(current->ldt[1]);
	data_base = code_base;
	set_base(current->ldt[1], code_base);
	set_limit(current->ldt[1], code_limit);
	set_base(current->ldt[2], data_base);
	set_limit(current->ldt[2], data_limit);
/* make sure fs points to the NEW data segment */
	/* FS段寄存器中放入局部表数据段描述符的选择符(0x17) */
	__asm__("pushl $0x17\n\tpop %%fs"::);
	/* 将参数和环境空间已存放数据的页面（最多有MAX_ARG_PAGES页）放到数据段末端 */
	data_base += data_limit - LIBRARY_SIZE; /* 库文件代码占用进程空间末端部分 */
	for (i = MAX_ARG_PAGES - 1; i >= 0; i--) {
		data_base -= PAGE_SIZE;
		if (page[i]) {
			put_dirty_page(page[i], data_base);
		}
	}
	return data_limit;
}

/*
 * 'do_execve()' executes a new program.
 *
 * NOTE! We leave 4MB free at the top of the data-area for a loadable
 * library.
 */
/*
 * 'do_execve()'函数执行一个新程序.
 */

/**
 * 加载并执行子进程(其他程序) 系统中断调用
 * 该函数是系统中断调用(int 0x80)功能号__NR_execve调用的函数。函数的参数是进入系统调用处理过程
 * 后直到调用本系统调用处理过程和调用本函数之前逐步压入栈中的值
 * @param[in]	eip			调用系统中断的程序代码指针
 * @param[in]	tmp			系统中断在调用sys_execve时的返回地址，无用
 * @param[in]	filename	被执行程序文件名指针
 * @param[in]	argv		命令行参数指针数组的指针
 * @param[in]	envp		环境变更指针数组的指针
 * @retval		调用成功返回0；失败设置出错号，并返回-1
 */
int do_execve(unsigned long * eip, long tmp, char * filename,
	char ** argv, char ** envp)
{
	struct m_inode * inode;
	struct buffer_head * bh;
	struct exec ex;
	unsigned long page[MAX_ARG_PAGES]; /* 参数和环境串空间页面指针数组 */
	int i, argc, envc;
	int e_uid, e_gid;
	int retval;
	int sh_bang = 0;
	unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4;

	/* 参数eip[1]是调用本次系统调用的原用户程序代码段寄存器CS值，其中的段选择符当然必须是
	当前任务的代码段选择符（0x000f）。 若不是该值，那么CS只能会是内核代码段的选择符0x0008。
	但这是绝对不允许的，因为内核代码是常驻内存而不能被替换掉的。*/
	if ((0xffff & eip[1]) != 0x000f) {
		panic("execve called from supervisor mode");
	}
	for (i = 0; i < MAX_ARG_PAGES; i++) {	/* clear page-table */
		page[i] = 0;
	}
	if (!(inode = namei(filename))) {		/* get executables inode */
		return -ENOENT;
	}
	argc = count(argv);
	envc = count(envp);
	
restart_interp:
	if (!S_ISREG(inode->i_mode)) {	/* must be regular file */
									/* 必须是常规文件 */
		retval = -EACCES;
		goto exec_error2;
	}
	i = inode->i_mode;
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid; /* 是否设置了执行时设置用户ID */
	e_gid = (i & S_ISGID) ? inode->i_gid : current->egid; /* 是否设置了执行时设置组ID */
	/* 根据执行文件i节点中的属性，看看本进程是否有权执行它 */
	if (current->euid == inode->i_uid) {
		i >>= 6;
	} else if (in_group_p(inode->i_gid)) {
		i >>= 3;
	}
	/* 当前用户无权限 | 文件可执行 | 当前用户是否是超级用户 */
	/* 0 			| 1 		| 0 */
	/* 0 			| 0 		| 1 */
	/* 0 			| 0 		| 0 */
	if (!(i & 1) && !((inode->i_mode & 0111) && suser())) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
	/* 读取第一块数据 */
	if (!(bh = bread(inode->i_dev, inode->i_zone[0]))) {
		retval = -EACCES;
		goto exec_error2;
	}

	ex = *((struct exec *) bh->b_data);	/* read exec-header */
	/* “#!”开头则为脚本文件 */
	if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
		/*
		 * This section does the #! interpretation.
		 * Sorta complicated, but hopefully it will work.  -TYT
		 */

		char buf[128], *cp, *interp, *i_name, *i_arg;
		unsigned long old_fs;

		strncpy(buf, bh->b_data + 2, 127);
		brelse(bh);
		iput(inode);
		buf[127] = '\0';
		if ((cp = strchr(buf, '\n'))) {
			*cp = '\0';
			for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp ++);
		}
		if (!cp || *cp == '\0') {
			retval = -ENOEXEC; /* No interpreter name found */
			goto exec_error1;
		}
		interp = i_name = cp;
		i_arg = 0;
		for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
 			if (*cp == '/') {
				i_name = cp + 1;
			}
		}
		if (*cp) {
			*cp++ = '\0';
			i_arg = cp;
		}
		/*
		 * OK, we've parsed out the interpreter name and
		 * (optional) argument.
		 */
		if (sh_bang++ == 0) {
			p = copy_strings(envc, envp, page, p, 0);
			p = copy_strings(--argc, argv+1, page, p, 0);
		}
		/*
		 * Splice in (1) the interpreter's name for argv[0]
		 *           (2) (optional) argument to interpreter
		 *           (3) filename of shell script
		 *
		 * This is done in reverse order, because of how the
		 * user environment and arguments are stored.
		 */
		/*
		* 拼接 	 (1) argv[0]中放解释程序的名称
		* 		(2) (可选的)解释程序的参数
		* 		(3) 脚本程序的名称
		*
		* 这是以逆序进行处理的，是由于用户环境和参数的存放方式造成的。
		*/
		p = copy_strings(1, &filename, page, p, 1);
		argc ++;
		if (i_arg) {
			p = copy_strings(1, &i_arg, page, p, 2);
			argc++;
		}
		p = copy_strings(1, &i_name, page, p, 2);
		argc ++;
		/* 上面一段代码的主要功能： 
			1. 复制环境变量
			2. 复制程序参数，举例如下：
				命令行： $ example.sh -arg1 -arg2
				example.sh文件第一句： #!/bin/bash -argv1 -argv2
		   	   进程空间末尾的参数和环境空间中则存入了（从低到高）：
		   		bash -argv1 -argv2 example.sh -arg1 -arg2
		*/
		if (!p) {
			retval = -ENOMEM;
			goto exec_error1;
		}
		/*
		 * OK, now restart the process with the interpreter's inode.
		 */
		/* namei是从用户数据空间（fs指向）取参数的，而interp处于内核数据空间，故临时设置fs
		 指向内核空间 */
		old_fs = get_fs();
		set_fs(get_ds());
		if (!(inode = namei(interp))) { /* get executables inode */
			set_fs(old_fs);
			retval = -ENOENT;
			goto exec_error1;
		}
		set_fs(old_fs);
		goto restart_interp;
	}
	brelse(bh);
	/* 对这个内核来说，它仅支持ZMAGIC执行文件格式，不支持含有代码或数据重定位信息的执
	行文件，执行文件实在太大或者执行文件残缺不全也不行 */
	if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
		ex.a_text + ex.a_data + ex.a_bss > 0x3000000 ||
		inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF(ex)) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (N_TXTOFF(ex) != BLOCK_SIZE) {
		printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (!sh_bang) {
		p = copy_strings(envc, envp, page, p, 0);
		p = copy_strings(argc, argv, page, p, 0);
		if (!p) {
			retval = -ENOMEM;
			goto exec_error2;
		}
	}
/* OK, This is the point of no return */
/* note that current->library stays unchanged by an exec */
/* OK，下面开始就没有返回的地方了 */
/* 注意，exec类函数不会改动 current->library */
	if (current->executable) {
		iput(current->executable);
	}
	current->executable = inode;
	current->signal = 0;
	/* 复位原进程的所有信号处理句柄，忽略SIG_IGN的句柄 */
	for (i = 0; i < 32; i ++) {
		current->sigaction[i].sa_mask = 0;
		current->sigaction[i].sa_flags = 0;
		if (current->sigaction[i].sa_handler != SIG_IGN) {
			current->sigaction[i].sa_handler = NULL;
		}
	}
	/* 根据close_on_exec关闭指定的文件，复位close_on_exec */
	for (i = 0; i < NR_OPEN; i ++) {
		if ((current->close_on_exec >> i) & 1) {
			sys_close(i);
		}
	}
	current->close_on_exec = 0;
	/* 释放原进程的代码段和数据段占用的物理页面及页表 */
	free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
	if (last_task_used_math == current) {
		last_task_used_math = NULL;
	}
	current->used_math = 0;
	p += change_ldt(ex.a_text, page);
	p -= LIBRARY_SIZE + MAX_ARG_PAGES * PAGE_SIZE;
	p = (unsigned long) create_tables((char *)p, argc, envc);
	current->brk = ex.a_bss +
		(current->end_data = ex.a_data +
		(current->end_code = ex.a_text));
	current->start_stack = p & 0xfffff000;
	current->suid = current->euid = e_uid;
	current->sgid = current->egid = e_gid;
	/* 将原调用系统中断的程序在堆栈上的代码指针替换为指向新执行程序的入口点，并将栈指针
	 替换为新执行文件的栈指针 */
	eip[0] = ex.a_entry;		/* eip, magic happens :-) */
	eip[3] = p;					/* stack pointer */
	return 0;
exec_error2:
	iput(inode);
exec_error1:
	for (i = 0; i < MAX_ARG_PAGES; i ++) {
		free_page(page[i]);
	}
	return(retval);
}
