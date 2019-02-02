#!/bin/sh

export OSLAB_PATH=$(dirname `which $0`)

bochs -q -f $OSLAB_PATH/bochs/bochsrc.bxrc
