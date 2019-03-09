#ifndef _CONST_H
#define _CONST_H

/* 定义缓冲使用内存的末端(代码中没有使用该常量) */
#define BUFFER_END 0x200000

/* i节点数据结构中i_mode字段的各标志位 */
#define I_TYPE          0170000		/* 指明i节点类型(类型屏蔽码) */
#define I_DIRECTORY		0040000		/* 目录文件 */
#define I_REGULAR       0100000		/* 常规文件，不是目录文件或特殊文件 */
#define I_BLOCK_SPECIAL 0060000		/* 块设备特殊文件 */
#define I_CHAR_SPECIAL  0020000		/* 字符设备特殊文件 */
#define I_NAMED_PIPE	0010000		/* 命名管道节点 */
#define I_SET_UID_BIT   0004000		/* 在执行时设置有效用户ID类型 */
#define I_SET_GID_BIT   0002000		/* 在执行时设置有效组ID类型 */

#endif
