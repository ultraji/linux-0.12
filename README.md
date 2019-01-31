# linux-0.12 源码学习

*参考《Linux内核完全剖析 --基于0.12内核》*

加入中文注释，方便阅读，并**修改部分代码使其能在现在的环境下编译**。

| 文件夹        | 说明                  |
| ------------ | -------------------- |
| `linux-0.12` | linux-0.12源代码      |
| `notes`      | 学习笔记              |
| `resources`  | 一些资源              |
| `oslab`      | 实验目录              |

## 搭建环境篇

### ubuntu (64bit >= 14.04)

ubuntu的用户可以使用`resources` 下的一键脚本 [setup.sh](resources\setup.sh)。

选项说明：

- 不带参数 安装编译环境, 安装 bochs 虚拟机
- `-e` 安装源码的编译环境(gcc-3.4)
- `-b` 安装 bochs 模拟器
- `-bm` 下载和编译 bochs 源码, 生成 bochs 模拟器 (不是必需)

### 其他系统 (包括ubuntu)

其他系统 (包括ubuntu) 的用户可以选择已创建好的 docker 镜像作为实验环境 (已安装 gcc-3.4 编译环境以及 bochs 模拟器)。docker 安装过程不再描述，支持 mac, windows, linux。

1. 首先从 docker hub 中拉取镜像;

    ```shell
    docker pull ultraji/ubuntu-xfce-novnc:os_learn 
    ```

2. 运行容器

    ```shell
    docker run -d -i -p 6080:6080 ultraji/ubuntu-xfce-novnc:os_learn 
    ```

## 笔记

1. [源代码文件树](notes/tree.md)

2. [常见编译问题总结](notes/make_problem.md) &emsp;如需对从 oldlinux 下载的 linux-0.1x 的代码进行修改, 可参考。