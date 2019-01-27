# linux-0.12 源码学习

*一步一步学linux学习 &emsp;-- 参考《Linux内核完全剖析 --基于0.12内核》*

加入中文注释，方便阅读，**并修改部分代码使其能在现在的环境下编译。**

| 文件夹        | 说明                  |
| ------------ | -------------------- |
| `linux-0.12` | linux-0.12源代码      |
| `notes`      | 学习笔记              |
| `resources`  | 一些资源              |

## 搭建环境篇

适合不局限于阅读源代码，有动手想法的小伙伴。

### 编译环境搭建

1. 方式一 (ubuntu 64bit >= 16.04)
    
    1. 下载并运行 `resources\` 下的一键安装脚本[setup.sh](resources\setup.sh);

2. 方式二 (使用docker容器)

    1. docker 安装过程不再描述，支持 mac, windows, linux, 从 docker hub 中拉取镜像;

        ```shell
        docker pull ultraji/ubuntu:os_learn
        ```

    2. 通过以下命令把源代码目录挂载到docker容器中编译

        ```shell
        docker run -t -i -v ${项目的本地路径}:${需要挂载到docker下的路径，例如/home/linux-0.12/} ultraji/ubuntu:os_learn 
        ```

说明 : 本项目目录下的linux-0.12已经能够编译通过了。[常见编译问题总结](notes/make_problem.md)适合对从oldlinux网站下载到的linux-0.1x的代码修改。

### bochs 模拟器

## 笔记

1. [源代码文件树](notes/tree.md)

2. [常见编译问题总结](notes/make_problem.md)