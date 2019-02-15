#!/bin/sh
export OSLAB_PATH=$(dirname `which $0`)

if [ "$1" ] && [ "$1" = "-m" ]
then
(cd ../linux-0.12; make; cp Image ../oslab/Image)
elif [ "$1" ] && [ "$1" = "-g" ]
then
$OSLAB_PATH/bochs/bochs-gdb -q -f $OSLAB_PATH/bochs/bochsrc-gdb.bxrc & \
gdb -x $OSLAB_PATH/bochs/gdb-cmd.txt ../linux-0.12/tools/system
else
$OSLAB_PATH/bochs/bochs-dbg -q -f $OSLAB_PATH/bochs/bochsrc.bxrc
fi