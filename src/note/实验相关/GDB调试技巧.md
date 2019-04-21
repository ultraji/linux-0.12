# gdb 调试技巧

## 调试命令

### 运行

- `r` 运行

- `c` 继续运行

### 设置断点

- `b breakpoint` 设置断点， 例如 `b main` 在main函数开始处设置断点

- `b linenum` 在当前调试文件的某行上设置断点，例如 `b 100` 在100行设置断点

- `b filename:linenum` 在某文件的某行上设置断点，例如 `b main.c:100` 在 main.c:100 行设置断点

### 单步调试

- `s` 单步调试（进入函数内部）

- `n` 单步调试（不进入函数内部）

- `until linenum` 选择循环结束的下一句的行号

- `finish` 跳出函数

- `set follow-fork-mode child` 设置gdb在fork之后跟踪子进程

- `set follow-fork-mode parent` 设置跟踪父进程。

### 打印

- `info reg` 打印寄存器内容
- `print x` 打印变量
- `bt`      打印堆栈
- `bt num`  num为正整数，打印栈顶上num层的栈信息；num为负整数，打印栈底下num层的栈信息。

#### 操作变量

- `print x=val` 修改变量值

### 退出

- `q` 退出

## 调试常见问题

### optimized out

**Q**：print 打印变量，输出 `<optimized out>` ？

**A**：原因：编译器优化掉了该变量，导致无法打印；解决：去除编译器优化命令，去掉gcc的`-O`选项，或指定为`-O0`。
