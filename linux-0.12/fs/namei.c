/*
 *  linux/fs/namei.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * Some corrections by tytso.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

/* 由文件名查找对应i节点的内部函数 */
static struct m_inode * _namei(const char * filename, struct m_inode * base,
    int follow_links);

// 例 "loveyou"[2] = 2["loveyou"] = *("loveyou"+2)= 'v'
/* 下面是访问模式宏。x是头文件include/fcntl.h中行7行开始定义的文件访问(打开)标志。这个宏根据文
 件访问标志x的值来索引双引号中对应的数值。双引号中有4个八进制数值(实际表示4个控制字符):
 "\004" - r		0
 "\002" - w		1
 "\006" - rw	2
 "\377" - wxrwxrwx  3
 例如，如果x为2，则该宏返回八进制值006，表示rw。另外，其中O_ACCMODE = 00003，防止数组访问越界。
*/
#define ACC_MODE(x) ("\004\002\006\377"[(x)&O_ACCMODE])

/*
 * comment out this line if you want names > NAME_LEN chars to be
 * truncated. Else they will be disallowed.
 */
/*
 * 如果想让文件名长度 > NAME_LEN个的字符被截掉，就将下面定义注释掉。
 */
/* #define NO_TRUNCATE */

#define MAY_EXEC 	1	/* 可执行 */
#define MAY_WRITE	2	/* 可写 */
#define MAY_READ 	4	/* 可读 */

/*
 *	permission()
 *
 * is used to check for read/write/execute permissions on a file.
 * I don't know if we should look at just the euid or both euid and
 * uid, but that should be easily changed.
 */
/*
 *	permission()
 * 该函数用于检测一个文件的读/写/执行权限。我不知道是否只需检查euid，还是需要检查euid和uid两者，
 * 不过这很容易修改。
 */

/**
 * 检测文件访问许可权限
 * @brief 文件访问属性字段 |...|r|w|x|r|w|x|r|w|x| 低九位从高到低，分别是用户、组、其他的读/写/执行
 * 
 * @param 	inode	文件的i节点指针
 * @param 	mask 	访问属性屏蔽码
 * @return 	访问许可返回1，否则返回0 
 */
static int permission(struct m_inode * inode, int mask)
{
    
    int mode = inode->i_mode;	/* 文件访问属性 */

    /* special case: not even root can read/write a deleted file */
    /* 特殊情况: 即使是超级用户(root)也不能读/写一个已被删除的文件. */
    if (inode->i_dev && !inode->i_nlinks) {
        return 0;
    } else if (current->euid == inode->i_uid) {	/* 同用户 */
        mode >>= 6;
    } else if (in_group_p(inode->i_gid)) {		/* 同组 */
        mode >>= 3;
    }

    if (((mode & mask & 0007) == mask) || suser()) {
        return 1;
    }
    return 0;
}

/*
 * ok, we cannot use strncmp, as the name is not in our data space.
 * Thus we'll have to use match. No big problem. Match also makes
 * some sanity tests.
 *
 * NOTE! unlike strncmp, match returns 1 for success, 0 for failure.
 */
/*
 * ok，我们不能使用strncmp字符串比较函数，因为名称不在我们的数据空间(不在内核空间)。因而我们只
 * 能使用match()。问题不大，match()同样也处理一些完整的测试。(注：第一句话的意思是因为
 * strncmp()比较用到的是DS和SS段寄存器，而在内核状态下，DS段指向的是内核空间，而name字符串不
 * 在内核空间。)
 *
 * 注意！与strncmp不同的是match()成功时返回1，失败时返回0。
 */

/**
 * 指定长度字符串比较函数
 * @param	len		比较的字符串长度
 * @param	name	文件名指针
 * @param	de		目录项结构
 * @return	相同返回1，不同返回0
 */
static int match(int len, const char * name, struct dir_entry * de)
{
    register int same __asm__("ax");

    /* 目录项指针空，或者目录项i节点等于0，或者要比较的字符串长度超过文件名长度 */
    if (!de || !de->inode || len > NAME_LEN) {
        return 0;
    }
    /* "" means "." ---> so paths like "/usr/lib//libc.a" work */
    /* ""当作"."来看待 ---> 这样就能处理象"/usr/lib//libc.a"那样的路径名 */
    if (!len && (de->name[0]=='.') && (de->name[1]=='\0')) {
        return 1;
    }
    /* 有点取巧，de->name[len] != '\0'说明de->name的长度大于len，长度不匹配 */
    if (len < NAME_LEN && de->name[len]) {
        return 0;
    }
    /* 进行快速比较操作 */
    __asm__(
        "cld\n\t"
        "fs ; repe ; cmpsb\n\t"
        "setz %%al"
        :"=a" (same)
        :"0" (0),"S" ((long) name),"D" ((long) de->name),"c" (len)
        );

    return same;
}

/*
 *	find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 *
 * This also takes care of the few special cases due to '..'-traversal
 * over a pseudo-root and a mount point.
 */
/*
 *	find_entry()
 *
 * 在指定目录中寻找一个与名字匹配的目录项。返回一个含有找到目录项的高速缓冲块以及目录项本身(作
 * 为一个参数--res_dir)。该函数并不读取目录项的i节点--如果需要的话则自己操作。
 *
 * 由于有'..'目录项，因此在操作期间也会对几种特殊情况分别处理--比如横越一个伪根目录以及安装点。
 */

/**
 * 查找指定目录和文件名的目录项
 * 该函数在指定目录的数据(文件)中搜索指定文件名的目录项。并对指定文件名是'..'的情况根据当前进
 * 行的相关设置进行特殊处理。
 * @param[in]		*dir	指定目录i节点的指针
 * @param[in]		name	文件名
 * @param[in]		namelen	文件名长度
 * @param[in/out]	res_dir 返回的指定目录项结构指针
 * @retval			成功则返回高速缓冲区指针，失败则返回空指针NULL
 */
static struct buffer_head * find_entry(struct m_inode ** dir,
    const char * name, int namelen, struct dir_entry ** res_dir)
{
    int entries;
    int block, i;
    struct buffer_head * bh;
    struct dir_entry * de;
    struct super_block * sb;

#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    entries = (*dir)->i_size / (sizeof (struct dir_entry));
    *res_dir = NULL;
    /* check for '..', as we might have to do some "magic" for it */
    /* 检查目录项'..',因为我们可能需要对其进行特殊处理 */
    // Q: 为什么不用name[0]和name[1]替代？
    // A: 因为name指针的内容不在内核空间，内核状态下fs指向用户空间。
    if (namelen == 2 && get_fs_byte(name) == '.' && get_fs_byte(name+1) == '.') {
    /* '..' in a pseudo-root results in a faked '.' (just change namelen) */
    /* 伪根中的'..'如同一个假'.'(只需改变名字长度) */
        if ((*dir) == current->root) {
            namelen = 1;
        } else if ((*dir)->i_num == ROOT_INO) {
    /* '..' over a mount-point results in 'dir' being exchanged for the mounted
        directory-inode. NOTE! We set mounted, so that we can iput the new dir */
            sb = get_super((*dir)->i_dev);
            if (sb->s_imount) {
                iput(*dir);
                (*dir)=sb->s_imount;
                (*dir)->i_count++;
            }
        }
    }
    if (!(block = (*dir)->i_zone[0])) {
        return NULL;
    }
    if (!(bh = bread((*dir)->i_dev,block))) {
        return NULL;
    }
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (i < entries) {
        if ((char *)de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            if (!(block = bmap(*dir,i/DIR_ENTRIES_PER_BLOCK)) ||
                !(bh = bread((*dir)->i_dev,block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        if (match(namelen,name,de)) {
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}

/*
 *	add_entry()
 *
 * adds a file entry to the specified directory, using the same
 * semantics as find_entry(). It returns NULL if it failed.
 *
 * NOTE!! The inode part of 'de' is left at 0 - which means you
 * may not sleep between calling this and putting something into
 * the entry, as someone else might have used it while you slept.
 */
/*
 * add_entry()
 * 使用与find_entry()同样的方法，往指定目录中添加一指定文件名的目录项。如果失败则返回NULL。
 *
 * 注意！！'de'（指定目录项结构指针）的i节点部分被设置为0 - 这表示在调用该函数和往目录项中添
 * 加信息之间不能去睡眠，因为如果睡眠，那么其他人（进程）可能会使用该目录项。
 */
/**
 * 根据指定的目录和文件名添加目录项
 * @param[in]		dir			指定目录的i节点
 * @param[in]		name		文件名
 * @param[in]		namelen		文件名长度
 * @param[in/out]	res_dir		返回的目录项结构指针
 * @retval			成功返回高速缓冲区指针，失败返回NULL
 */
static struct buffer_head * add_entry(struct m_inode * dir,
    const char * name, int namelen, struct dir_entry ** res_dir)
{
    int block,i;
    struct buffer_head * bh;
    struct dir_entry * de;

    *res_dir = NULL;
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    if (!namelen) {
        return NULL;
    }
    if (!(block = dir->i_zone[0])) {
        return NULL;
    }
    if (!(bh = bread(dir->i_dev,block))) {
        return NULL;
    }
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (1) {
        if ((char *)de >= BLOCK_SIZE+bh->b_data) {
            brelse(bh);
            bh = NULL;
            block = create_block(dir,i/DIR_ENTRIES_PER_BLOCK);
            if (!block)
                return NULL;
            if (!(bh = bread(dir->i_dev,block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        if (i*sizeof(struct dir_entry) >= dir->i_size) {
            de->inode=0;
            dir->i_size = (i+1)*sizeof(struct dir_entry);
            dir->i_dirt = 1;
            dir->i_ctime = CURRENT_TIME;
        }
        if (!de->inode) {
            dir->i_mtime = CURRENT_TIME;
            for (i=0; i < NAME_LEN ; i++)
                de->name[i]=(i<namelen)?get_fs_byte(name+i):0;
            bh->b_dirt = 1;
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}


/**
 * 查找符号链接的i节点
 * @param[in]	dir		目录i节点
 * @param[in]	inode	目录项i节点
 * @retval		返回符号链接到文件的i节点指针，出错返回NULL
 */
static struct m_inode * follow_link(struct m_inode * dir, struct m_inode * inode)
{
    unsigned short fs;
    struct buffer_head * bh;

    if (!dir) {
        dir = current->root;
        dir->i_count ++;
    }
    if (!inode) {
        iput(dir);
        return NULL;
    }
    if (!S_ISLNK(inode->i_mode)) {
        iput(dir);
        return inode;
    }
    __asm__("mov %%fs,%0":"=r" (fs));
    if (fs != 0x17 || !inode->i_zone[0] ||
       !(bh = bread(inode->i_dev, inode->i_zone[0]))) {
        iput(dir);
        iput(inode);
        return NULL;
    }
    iput(inode);
    __asm__("mov %0,%%fs"::"r" ((unsigned short) 0x10));
    inode = _namei(bh->b_data,dir,0);
    __asm__("mov %0,%%fs"::"r" (fs));
    brelse(bh);
    return inode;
}

/*
 *	get_dir()
 *
 * Getdir traverses the pathname until it hits the topmost directory.
 * It returns NULL on failure.
 */
/*
 *	get_dir()
 *
 * 该函数根据给出的路径名进行搜索，直到达到最顶端的目录。如果失败，返回NULL。
 */
/**
 * 从指定目录开始搜寻指定路径名的目录(或文件名)的i节点
 * @param	pathname    路径名
 * @param	inode       指定起始目录的i节点
 * @return	成功返回目录或文件的i节点指针，失败时返回NULL.
 */
static struct m_inode * get_dir(const char * pathname, struct m_inode * inode)
{
    char c;
    const char * thisname;
    struct buffer_head * bh;
    int namelen,inr;
    struct dir_entry * de;
    struct m_inode * dir;

    if (!inode) {
        inode = current->pwd;
        inode->i_count++;
    }
    if ((c=get_fs_byte(pathname))=='/') {
        iput(inode);
        inode = current->root;
        pathname++;
        inode->i_count++;
    }
    while (1) {
        thisname = pathname;
        if (!S_ISDIR(inode->i_mode) || !permission(inode,MAY_EXEC)) {
            iput(inode);
            return NULL;
        }
        for(namelen=0;(c=get_fs_byte(pathname++))&&(c!='/');namelen++)
            /* nothing */ ;
        if (!c)
            return inode;
        if (!(bh = find_entry(&inode,thisname,namelen,&de))) {
            iput(inode);
            return NULL;
        }
        inr = de->inode;
        brelse(bh);
        dir = inode;
        if (!(inode = iget(dir->i_dev,inr))) {
            iput(dir);
            return NULL;
        }
        if (!(inode = follow_link(dir,inode)))
            return NULL;
    }
}

/*
 *	dir_namei()
 *
 * dir_namei() returns the inode of the directory of the
 * specified name, and the name within that directory.
 */
/*
 *	dir_namei()
 *
 * dir_namei()函数返回指定目录名的i节点指针，以及在最顶层目录的名称。
 */
/**
 * 获取指定目录名的i节点指针，以及在最顶层目录的名称
 * @note	这里"最顶层目录"是指路径名中最靠近末端的目录
 * @param	pathname	目录路径名
 * @param	namelen		路径名长度
 * @param	name		返回的最顶层目录名
 * @param	base		搜索起始目录的i节点
 * @retval	成功返回指定目录名最顶层的i节点指针，出错时返回NULL。
 */
static struct m_inode * dir_namei(const char * pathname,
    int * namelen, const char ** name, struct m_inode * base)
{
    char c;
    const char * basename;
    struct m_inode * dir;

    if (!(dir = get_dir(pathname, base))) {
        return NULL;
    }
    basename = pathname;
    while ((c = get_fs_byte(pathname++))) {
        if (c == '/') {
            basename = pathname;
        }
    }
    *namelen = pathname - basename - 1;
    *name = basename;
    return dir;
}

/**
 * 取指定路径名的i节点
 * @param[in]	pathname		路径名
 * @param[in]	base			搜索起点目录i节点
 * @param[in]	follow_links	是否跟随符号链接的标志，1 - 需要，0 - 不需要
 * @retval		成功返回指定路径名的inode，失败返回NULL
 */
struct m_inode * _namei(const char * pathname, struct m_inode * base,
    int follow_links)
{
    const char * basename;
    int inr,namelen;
    struct m_inode * inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if (!(base = dir_namei(pathname, &namelen, &basename, base))) {
        return NULL;
    }
    if (!namelen) {			/* special case: '/usr/' etc */
        return base;
    }
    bh = find_entry(&base, basename, namelen, &de);
    if (!bh) {
        iput(base);
        return NULL;
    }
    inr = de->inode;
    brelse(bh);
    if (!(inode = iget(base->i_dev, inr))) {
        iput(base);
        return NULL;
    }
    if (follow_links) {
        inode = follow_link(base, inode);
    } else {
        iput(base);
    }
    inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;
    return inode;
}


/**
 * 取指定路径名的i节点，不跟随符号链接
 * @param[in]	pathname	路径名
 * @retval		成功返回对应的i节点，失败返回NULL
*/
struct m_inode * lnamei(const char * pathname)
{
    return _namei(pathname, NULL, 0);
}

/*
 *	namei()
 *
 * is used by most simple commands to get the inode of a specified name.
 * Open, link etc use their own routines, but this is enough for things
 * like 'chmod' etc.
 */
/*
 *	namei()
 *
 * 该函数被许多简单命令用于取得指定路径名称的i节点。open,link等则使用它们自己的相应函数。但对于
 * 像修改模式"chmod"等这样的命令，该函数已足够用了。
 */
/**
 * 取指定路径名的i节点,跟随符号链接
 * @param[in]	pathname	路径名
 * @retval		成功返回对应的i节点，失败返回NULL
 */
struct m_inode * namei(const char * pathname)
{
    return _namei(pathname, NULL, 1);
}

/*
 *	open_namei()
 *
 * namei for open - this is in fact almost the whole open-routine.
 */
/*
 * open()函数使用的namei函数 - 这其实几乎是完整的打开文件程序。
 */

/**
 * 文件打开namei函数
 * @param[in]	filename	文件名
 * @param[in]	flag		打开文件标志
 * @param[in]	mode		指定文件的许可属性
 * @param[in]	res_inode	返回对应文件路径名的i节点指针
 * @retval		成功返回0，失败返回出错码
 */
int open_namei(const char * pathname, int flag, int mode,
    struct m_inode ** res_inode)
{
    const char * basename;
    int inr,dev,namelen;
    struct m_inode * dir, *inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if ((flag & O_TRUNC) && !(flag & O_ACCMODE)) {
        flag |= O_WRONLY;
    }
    mode &= 0777 & ~current->umask;
    mode |= I_REGULAR;
    if (!(dir = dir_namei(pathname,&namelen,&basename,NULL))) {
        return -ENOENT;
    }
    if (!namelen) {			/* special case: '/usr/' etc */
        if (!(flag & (O_ACCMODE|O_CREAT|O_TRUNC))) {
            *res_inode=dir;
            return 0;
        }
        iput(dir);
        return -EISDIR;
    }
    bh = find_entry(&dir, basename, namelen, &de);
    if (!bh) {
        if (!(flag & O_CREAT)) {
            iput(dir);
            return -ENOENT;
        }
        if (!permission(dir,MAY_WRITE)) {
            iput(dir);
            return -EACCES;
        }
        inode = new_inode(dir->i_dev);
        if (!inode) {
            iput(dir);
            return -ENOSPC;
        }
        inode->i_uid = current->euid;
        inode->i_mode = mode;
        inode->i_dirt = 1;
        bh = add_entry(dir, basename, namelen, &de);
        if (!bh) {
            inode->i_nlinks--;
            iput(inode);
            iput(dir);
            return -ENOSPC;
        }
        de->inode = inode->i_num;
        bh->b_dirt = 1;
        brelse(bh);
        iput(dir);
        *res_inode = inode;
        return 0;
    }
    inr = de->inode;
    dev = dir->i_dev;
    brelse(bh);
    if (flag & O_EXCL) {
        iput(dir);
        return -EEXIST;
    }
    if (!(inode = follow_link(dir,iget(dev,inr)))) {
        return -EACCES;
    }
    if ((S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)) ||
        !permission(inode,ACC_MODE(flag))) {
        iput(inode);
        return -EPERM;
    }
    inode->i_atime = CURRENT_TIME;
    if (flag & O_TRUNC) {
        truncate(inode);
    }
    *res_inode = inode;
    return 0;
}

int sys_mknod(const char * filename, int mode, int dev)
{
    const char * basename;
    int namelen;
    struct m_inode * dir, * inode;
    struct buffer_head * bh;
    struct dir_entry * de;
    
    if (!suser()) {
        return -EPERM;
    }
    if (!(dir = dir_namei(filename,&namelen,&basename, NULL))) {
        return -ENOENT;
    }
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    bh = find_entry(&dir,basename,namelen,&de);
    if (bh) {
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }
    inode = new_inode(dir->i_dev);
    if (!inode) {
        iput(dir);
        return -ENOSPC;
    }
    inode->i_mode = mode;
    if (S_ISBLK(mode) || S_ISCHR(mode)) {
        inode->i_zone[0] = dev;
    }
    inode->i_mtime = inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;
    bh = add_entry(dir,basename,namelen,&de);
    if (!bh) {
        iput(dir);
        inode->i_nlinks=0;
        iput(inode);
        return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;
    iput(dir);
    iput(inode);
    brelse(bh);
    return 0;
}

int sys_mkdir(const char * pathname, int mode)
{
    const char * basename;
    int namelen;
    struct m_inode * dir, * inode;
    struct buffer_head * bh, *dir_block;
    struct dir_entry * de;

    if (!(dir = dir_namei(pathname,&namelen,&basename, NULL))) {
        return -ENOENT;
    }
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    bh = find_entry(&dir,basename,namelen,&de);
    if (bh) {
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }
    inode = new_inode(dir->i_dev);
    if (!inode) {
        iput(dir);
        return -ENOSPC;
    }
    inode->i_size = 32;
    inode->i_dirt = 1;
    inode->i_mtime = inode->i_atime = CURRENT_TIME;
    if (!(inode->i_zone[0]=new_block(inode->i_dev))) {
        iput(dir);
        inode->i_nlinks--;
        iput(inode);
        return -ENOSPC;
    }
    inode->i_dirt = 1;
    if (!(dir_block=bread(inode->i_dev,inode->i_zone[0]))) {
        iput(dir);
        inode->i_nlinks--;
        iput(inode);
        return -ERROR;
    }
    de = (struct dir_entry *) dir_block->b_data;
    de->inode=inode->i_num;
    strcpy(de->name,".");
    de++;
    de->inode = dir->i_num;
    strcpy(de->name,"..");
    inode->i_nlinks = 2;
    dir_block->b_dirt = 1;
    brelse(dir_block);
    inode->i_mode = I_DIRECTORY | (mode & 0777 & ~current->umask);
    inode->i_dirt = 1;
    bh = add_entry(dir,basename,namelen,&de);
    if (!bh) {
        iput(dir);
        inode->i_nlinks=0;
        iput(inode);
        return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;
    dir->i_nlinks++;
    dir->i_dirt = 1;
    iput(dir);
    iput(inode);
    brelse(bh);
    return 0;
}

/*
 * routine to check that the specified directory is empty (for rmdir)
 */
static int empty_dir(struct m_inode * inode)
{
    int nr,block;
    int len;
    struct buffer_head * bh;
    struct dir_entry * de;

    len = inode->i_size / sizeof (struct dir_entry);
    if (len<2 || !inode->i_zone[0] ||
        !(bh=bread(inode->i_dev,inode->i_zone[0]))) {
            printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return 0;
    }
    de = (struct dir_entry *) bh->b_data;
    if (de[0].inode != inode->i_num || !de[1].inode || 
        strcmp(".",de[0].name) || strcmp("..",de[1].name)) {
            printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return 0;
    }
    nr = 2;
    de += 2;
    while (nr<len) {
        if ((void *) de >= (void *) (bh->b_data+BLOCK_SIZE)) {
            brelse(bh);
            block=bmap(inode,nr/DIR_ENTRIES_PER_BLOCK);
            if (!block) {
                nr += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            if (!(bh=bread(inode->i_dev,block)))
                return 0;
            de = (struct dir_entry *) bh->b_data;
        }
        if (de->inode) {
            brelse(bh);
            return 0;
        }
        de++;
        nr++;
    }
    brelse(bh);
    return 1;
}

int sys_rmdir(const char * name)
{
    const char * basename;
    int namelen;
    struct m_inode * dir, * inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if (!(dir = dir_namei(name,&namelen,&basename, NULL)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    bh = find_entry(&dir,basename,namelen,&de);
    if (!bh) {
        iput(dir);
        return -ENOENT;
    }
    if (!(inode = iget(dir->i_dev, de->inode))) {
        iput(dir);
        brelse(bh);
        return -EPERM;
    }
    if ((dir->i_mode & S_ISVTX) && current->euid &&
        inode->i_uid != current->euid) {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (inode->i_dev != dir->i_dev || inode->i_count>1) {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (inode == dir) {	/* we may not delete ".", but "../dir" is ok */
        iput(inode);
        iput(dir);
        brelse(bh);
        return -EPERM;
    }
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        iput(dir);
        brelse(bh);
        return -ENOTDIR;
    }
    if (!empty_dir(inode)) {
        iput(inode);
        iput(dir);
        brelse(bh);
        return -ENOTEMPTY;
    }
    if (inode->i_nlinks != 2)
        printk("empty directory has nlink!=2 (%d)",inode->i_nlinks);
    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    inode->i_nlinks=0;
    inode->i_dirt=1;
    dir->i_nlinks--;
    dir->i_ctime = dir->i_mtime = CURRENT_TIME;
    dir->i_dirt=1;
    iput(dir);
    iput(inode);
    return 0;
}

int sys_unlink(const char * name)
{
    const char * basename;
    int namelen;
    struct m_inode * dir, * inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if (!(dir = dir_namei(name,&namelen,&basename, NULL)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    bh = find_entry(&dir,basename,namelen,&de);
    if (!bh) {
        iput(dir);
        return -ENOENT;
    }
    if (!(inode = iget(dir->i_dev, de->inode))) {
        iput(dir);
        brelse(bh);
        return -ENOENT;
    }
    if ((dir->i_mode & S_ISVTX) && !suser() &&
        current->euid != inode->i_uid &&
        current->euid != dir->i_uid) {
        iput(dir);
        iput(inode);
        brelse(bh);
        return -EPERM;
    }
    if (S_ISDIR(inode->i_mode)) {
        iput(inode);
        iput(dir);
        brelse(bh);
        return -EPERM;
    }
    if (!inode->i_nlinks) {
        printk("Deleting nonexistent file (%04x:%d), %d\n",
            inode->i_dev,inode->i_num,inode->i_nlinks);
        inode->i_nlinks=1;
    }
    de->inode = 0;
    bh->b_dirt = 1;
    brelse(bh);
    inode->i_nlinks--;
    inode->i_dirt = 1;
    inode->i_ctime = CURRENT_TIME;
    iput(inode);
    iput(dir);
    return 0;
}

int sys_symlink(const char * oldname, const char * newname)
{
    struct dir_entry * de;
    struct m_inode * dir, * inode;
    struct buffer_head * bh, * name_block;
    const char * basename;
    int namelen, i;
    char c;

    dir = dir_namei(newname,&namelen,&basename, NULL);
    if (!dir)
        return -EACCES;
    if (!namelen) {
        iput(dir);
        return -EPERM;
    }
    if (!permission(dir,MAY_WRITE)) {
        iput(dir);
        return -EACCES;
    }
    if (!(inode = new_inode(dir->i_dev))) {
        iput(dir);
        return -ENOSPC;
    }
    inode->i_mode = S_IFLNK | (0777 & ~current->umask);
    inode->i_dirt = 1;
    if (!(inode->i_zone[0]=new_block(inode->i_dev))) {
        iput(dir);
        inode->i_nlinks--;
        iput(inode);
        return -ENOSPC;
    }
    inode->i_dirt = 1;
    if (!(name_block=bread(inode->i_dev,inode->i_zone[0]))) {
        iput(dir);
        inode->i_nlinks--;
        iput(inode);
        return -ERROR;
    }
    i = 0;
    while (i < 1023 && (c=get_fs_byte(oldname++)))
        name_block->b_data[i++] = c;
    name_block->b_data[i] = 0;
    name_block->b_dirt = 1;
    brelse(name_block);
    inode->i_size = i;
    inode->i_dirt = 1;
    bh = find_entry(&dir,basename,namelen,&de);
    if (bh) {
        inode->i_nlinks--;
        iput(inode);
        brelse(bh);
        iput(dir);
        return -EEXIST;
    }
    bh = add_entry(dir,basename,namelen,&de);
    if (!bh) {
        inode->i_nlinks--;
        iput(inode);
        iput(dir);
        return -ENOSPC;
    }
    de->inode = inode->i_num;
    bh->b_dirt = 1;
    brelse(bh);
    iput(dir);
    iput(inode);
    return 0;
}

/**
 * 为文件建立一个文件名目录项
 * @brief 为一个已存在的文件创建一个新链接（也称为硬连接 - hard link）
 * 
 * @param 	oldname	原路径名
 * @param 	newname 新路径名
 * @return 	成功返回0，失败返回出错号
 */
int sys_link(const char * oldname, const char * newname)
{
    struct dir_entry * de;
    struct m_inode * oldinode, * dir;
    struct buffer_head * bh;
    const char * basename;
    int namelen;

    oldinode = namei(oldname);
    if (!oldinode) {
        return -ENOENT;
    }
    if (S_ISDIR(oldinode->i_mode)) {
        iput(oldinode);
        return -EPERM;
    }
    dir = dir_namei(newname, &namelen, &basename, NULL);
    if (!dir) {
        iput(oldinode);
        return -EACCES;
    }
    if (!namelen) {
        iput(oldinode);
        iput(dir);
        return -EPERM;
    }
    if (dir->i_dev != oldinode->i_dev) {
        iput(dir);
        iput(oldinode);
        return -EXDEV;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        iput(oldinode);
        return -EACCES;
    }
    bh = find_entry(&dir, basename, namelen, &de);
    if (bh) {
        brelse(bh);
        iput(dir);
        iput(oldinode);
        return -EEXIST;
    }
    bh = add_entry(dir, basename, namelen, &de);
    if (!bh) {
        iput(dir);
        iput(oldinode);
        return -ENOSPC;
    }
    de->inode = oldinode->i_num;
    bh->b_dirt = 1;
    brelse(bh);
    iput(dir);
    oldinode->i_nlinks ++;
    oldinode->i_ctime = CURRENT_TIME;
    oldinode->i_dirt = 1;
    iput(oldinode);
    return 0;
}
