# C代码阅读提示

1. ```volatile``` 关键字

    例如，在```include/linux/kernel.h```中有这样一行代码：

    ```volatile void do_exit(long error_code);```

    这里其实是帮助编译器进行优化，对于```do_exit()```而言，它是永远都不会返回的。如果还将调用它的函数的返回地址保存在堆栈上的话，是没有任何意义的。但是加了```volatile```过后，就意味着这个函数不会返回，就相当于告诉编译器，我调用后是不用保存调用我的函数的返回地址的。这样就达到了优化的作用。这种优化来源于gcc，在gcc2.5版本以后，使用```noreturn```属性来做优化，原代码等同于```void do_exit(int error_code) __attribute__((noreturn));```，但是在gcc2.5的版本以前，没有```noreturn```属性。
    