# gdb 调试技巧

1. `r` 运行

2. `c` 继续运行

3. `b breakpoint` 设置断点， 例如 `b main` 在main函数开始处设置断点

4. `s` 单步调试（进入函数内部）

5. `n` 单步调试（不进入函数内部）

6. `finish` 跳出函数

7. `set follow-fork-mode child` 设置gdb在fork之后跟踪子进程

    `set follow-fork-mode parent` 设置跟踪父进程。

8. `print x=val` 修改变量值

9. `q` 退出