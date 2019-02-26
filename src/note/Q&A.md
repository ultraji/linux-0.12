# Q & A

1. ```inode.c```中的```unlock_inode()```沒有关闭中断，```super.c```中的```free_super()```却关闭了中断？