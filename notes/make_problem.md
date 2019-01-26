# 编译源码时碰到的问题记录

1. ```make: as86: Command not finded```

    - 原因 : 
    
        as86 汇编器未安装

    - 解决 : 

        ```shell
        sudo apt-get install bin86
        ```

2. ```gas: Command not finded```

    - 原因 : 
    
        gas、gld 的名称已经过时

    - 解决 ：
        
        修改主 Makefile 文件

        将 `AS =gas` 修改为 `AS =as`

        将 `LD =gld` 修改为 `LD =ld`

3. ```Error: unsupported instruction `mov'```

    - 原因 :

        在64位机器上编译，需要告诉编译器要编译32位的code

    - 解决 :

        修改主 Makefile 文件

        将 `AS =gas` 修改为 `AS =as -32`

4. ```Error: alignment not a power of 2```

    - 原因 : 

        现在GNU as直接是写出对齐的值而非2的次方值了

    - 解决 : 

        找到对应位置

        `.align n` 应该改为 `.align 2^n`, 例如 `.align 3` 应该改为 `.align 8`

5. ```error: invalid option `string-insns'```  
```error: unrecognized command line option "-fcombine-regs"```

    - 原因 : 

        现在GCC已经不支持了

    - 解决 ：

        修改主 Makefile 文件

        将 `CFLAGS  =` 中的 `-fcombine-regs -mstring-insns` 注释掉或删掉

6. ```error: can't find a register in class `AREG' while reloading `asm'```

    - 原因 : 

        as的不断改进，不需要人工指定一个变量需要的CPU寄存器。代码中所有的 `__asm__("ax")`都需要去掉。

    - 解决 :

        例如
        ```asm
        :"si","di","ax","cx");
        ```
        改为
        ```asm
        :);
        ```