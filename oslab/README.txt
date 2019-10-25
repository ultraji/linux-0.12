# 实验篇

1. shell 脚本问题

*.bxrc 文件中的变量不要用`{}`包起来，例如要用`$OSLAB_PATH`，不要用`${OSLAB_PATH}`。否则会找不到变量。

2. 如何使bochs、gdb调试忽略page fault信号？

Created a patch "gdbstub.cc.patch" against bochs (version CVS 20080110)
   Bochs always tries to find out the reason of an exception, so that it can generate the right signal for gdb.
   If it fails to find a reason, bochs assigns a value GDBSTUB_STOP_NO_REASON (see bochs.h), which causes
   debug_loop() (see gdbstub.cc) to generate a signal of number 0.
   Signal 0 is problematic to gdb, as gdb doesn't allow us to ignore it.
   Somehow when we simulate linux, we get tons of signal 0's that seem to be caused by page faults.
   This patch makes bochs send SIGSEGV instead of signal 0, so that we can ignore it in gdb.

gdbstub.cc
debug_loop()

*** gdbstub.cc.orig Thu Oct 18 18:44:38 2007
--- gdbstub.cc Sat Jan 12 17:25:22 2008
*************** static void debug_loop(void)
*** 489,494 ****
--- 489,498 ----
          {
            write_signal(&buf[1], SIGTRAP);
          }
+  else if (last_stop_reason == GDBSTUB_STOP_NO_REASON)
+  {
+    write_signal(&buf[1], SIGSEGV);
+  }
          else
          {
            write_signal(&buf[1], 0);
