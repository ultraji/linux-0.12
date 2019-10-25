#!/bin/sh

vncserver -kill :1
vncserver -geometry 1280x1024 &> /dev/null
./.novnc/utils/launch.sh --vnc localhost:5901 &> /dev/null && /bin/sh