#!/bin/sh
export OSLAB_PATH=$(dirname `which $0`)

if [ ! -e "hdc.img" ]; then
tar -xvJf hdc.tar.xz
fi

if [ "$1" ] && [ "$1" = "-m" ]
then
(cd ../linux-0.12; make clean; make; cp Image ../oslab/Image)
elif [ "$1" ] && [ "$1" = "-g" ]
then
$OSLAB_PATH/bochs/bochs-gdb -q -f $OSLAB_PATH/bochs/bochsrc-gdb.bxrc & \
gdb -x $OSLAB_PATH/bochs/.gdbrc ../linux-0.12/tools/system
else
bochs -q -f $OSLAB_PATH/bochs/bochsrc.bxrc
fi