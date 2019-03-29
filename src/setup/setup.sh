#!/bin/bash

_echo_info(){
    echo -e "[Info]: ${1}"
}

_echo_err(){
    echo -e "\033[31m[Error]: ${1} \033[0m"
}

_echo_succ(){
    echo -e "\033[32m[Succ]: ${1} \033[0m"
}

bochs_make_install(){

    sudo apt-get install -y build-essential libgtk2.0-dev \
        libx11-dev xserver-xorg-dev xorg-dev g++ \
        pkg-config libxcursor-dev libxrandr-dev \
        libxinerama-dev libxi-dev &> /dev/null

    # libgtk2.0-dev 因为依赖问题安装失败的解决方案
    # sudo apt-get install aptitude && sudo aptitude install libgtk2.0-dev

    if [ ! -e "bochs-2.6.9.tar.gz" ]; then
        wget https://downloads.sourceforge.net/project/bochs/bochs/2.6.9/bochs-2.6.9.tar.gz -q --show-progress  && \
        _echo_succ "Download bochs-2.6.9.tar.gz Sucessfully." || (rm bochs-2.6.9.tar.gz & _echo_err "Download bochs-2.6.9.tar.gz unsuccessfully!!!" )
    fi
    tar zxvf bochs-2.6.9.tar.gz &> /dev/null && \
    _echo_info "tar bochs-2.6.9.tar.gz Sucessfully." || \
    (rm -rf ../bochs-2.6.9 & _echo_err "tar bochs-2.6.9.tar.gz unsuccessfully!!!" )

    if [ -d "bochs-2.6.9" ];then
        cd bochs-2.6.9
        if [ "$1" ] && [ "$1" = "-g" ]
        then
            ./configure --enable-gdb-stub --enable-disasm
        else
            ./configure --enable-debugger --enable-disasm
        fi
        make && \
        sudo make install
    fi

}

bochs_install(){
    sudo apt-get install -y build-essential &> /dev/null
    sudo apt-get install -y bochs bochs-x bochs-sdl &> /dev/null \
    && _echo_succ "bochs is installed." || _echo_err "bochs is not installed!!!"
}

env_install(){

    sudo apt-get install -y make bin86 gcc-multilib &> /dev/null \
    && _echo_succ "bin86 is installed." || _echo_err "bin86 is not installed!!!"

    if [ ! -z `which gcc-3.4` ];then
        _echo_succ "Gcc-3.4 is installed."
        return
    fi

    GCC_DIR="gcc-3.4"

    DOWNLOAD_LIST=(
        "gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb"
        "gcc-3.4_3.4.6-6ubuntu3_amd64.deb"
        "cpp-3.4_3.4.6-6ubuntu3_amd64.deb"
        "g++-3.4_3.4.6-6ubuntu3_amd64.deb"
        "libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb"
    )

    if [ -z `which gcc-3.4` ]; then
        _echo_info "Start installing gcc-3.4..."
        for deb in ${DOWNLOAD_LIST[*]}; do
            if [ ! -e ${GCC_DIR}/${deb} ]; then
                wget http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/${deb} -P ${GCC_DIR} -q --show-progress && \
                _echo_info "Download ${deb} Sucessfully." || ( rm ${deb} & _echo_err "Download ${deb} unsuccessfully!!!" )
            fi
        done
        sudo dpkg -i ${GCC_DIR}/*.deb &> /dev/null
        sudo apt-get install -y -f &> /dev/null
        if [ ! -z `which gcc-3.4` ];then
            _echo_succ "gcc-3.4 is installed."
        fi
        rm -rf ${GCC_DIR}
    fi
}

trap 'onCtrlC' INT
function onCtrlC () {
    _echo_err "[Warning]: stopped by user."
    rm -rf ${GCC_DIR}
    exit
}

if [ "$1" ] && [ "$1" = "-b" ]
then
    bochs_install
elif [ "$1" ] && [ "$1" = "-bm" ]
then
    bochs_make_install $2
elif [ "$1" ] && [ "$1" = "-e" ]
then
    env_install
else
    env_install
    bochs_install
fi