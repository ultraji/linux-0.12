# Bochs调试技巧

## 执行控制指令

1. 连续执行

    ```c/cont/continue```

2. 执行count条指令，默认为1条，会跟进到函数和中断调用的内部

    ```s/step/stepi [count]```

3. 执行count条指令，默认为1条，但跳过函数和中断调用

    ```p/n/next [count]```

4. 停止执行，并回到命令行提示符下

    ```Ctrl+C```

5. 退出调试和执行

    ```q/quit/exit```

## 断点设置命令

1. 在虚拟地址上设置指令断点，其中seg和offset可以是以0x开始的十六进制数，或十进制，或者是以0开头的八进制数

    ```vb/vbreak seg:offset```

2. 在线性地址上设置断点，addr同上面的seg和offset

    ```lb/lbreak addr```

3. 在物理地址上设置断点

    ```b/break/pb/pbreak addr```


4. 显示当前所有断点的信息 

    ```info break```

5. 删除一个断点

    ```d/del/delete n```


## 内存操作指令

1. 检查位于线性地址addr处的内存内容

    ```x /nuf addr```

2. 检查位于物理地址addr处的内存内容

    ```xp /nuf addr```

    其中参数n、u、f分别表示：

    - n 为要显示内存单元的计数值，默认为1

    - u 表示单元大小，默认值为w

        - b（bytes）		1字节
        - h（halfwords）	2字节
        - w（words）		4字节
        - g（giantwords）	8字节

    - f 为显示格式，默认为x

        - x（hex）         显示为十六进制数
        - d（decimal）	　　显示为十进制数
        - u（unsigned）    显示为无符号十进制数
        - o（octal）	   显示为八进制数
        - t（binary）	   显示为二进制数
        - c（char）  	   显示为对应的字符

## 信息显示和CPU寄存器操作命令

1. 列表显示CPU寄存器及其内容

    ```r/reg/regs/registers```

2. 修改某寄存器的内容。除段寄存器和标志寄存器以外的寄存器都可以修改，如set $eax=0x012345

    ```set $reg=val```

3. 列出所有的CR0-CR4寄存器

    ```creg```

4. 列出CPU全部状态信息，包括各个段选择子（cs，ds等）以及ldtr和gdtr等

    ```sreg```                        

5. 打印堆栈情况

    ```print-stack```

6. 显示页表

    ```info tab```


## 反汇编命令

1. 反汇编给定线性地址范围的指令。也可以是u /10 反汇编从当前地址开始的10条指令
    
    ```u/disasm/disassemble start end```