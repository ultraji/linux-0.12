#/bin/bash

bochs_install(){
    sudo apt-get install -y build-essential
    sudo apt-get install -y bochs bochs-x bochs-sdl
}

bochs_make_install(){
    DIR="bochs"
    BOCHS="bochs-2.6.9"

    INSTALL_LIST=("build-essential" "libgtk2.0-dev" "libx11-dev" "xserver-xorg-dev" "xorg-dev" "g++" \
        "pkg-config" "libxcursor-dev" "libxrandr-dev" "libxinerama-dev" "libxi-dev")

    for i in ${INSTALL_LIST[*]}; do 
        ( dpkg -l |grep ${i} &> /dev/null   \
        || (echo "[Info]: Installing ${i} ..." & sudo apt-get install -y ${i}))    \
        && echo "[Info]: ${i} is installed." || echo "[Error]: ${i} is not installed."
    done

    # libgtk2.0-dev 因为依赖问题安装失败的解决方案
    # dpkg -l |grep libgtk2.0-dev &> /dev/null || (sudo apt-get install aptitude && sudo aptitude install libgtk2.0-dev)

    if [ ! -d "${DIR}/${BOCHS}" ];then
        if [ ! -d "${DIR}" ];then
            mkdir ${DIR}
        fi
        cd ${DIR}
        if [ ! -e "${BOCHS}.tar.gz" ]; then
            echo "[Info]: downloading ${BOCHS}.tar.gz..."
            wget -qO- https://downloads.sourceforge.net/project/bochs/bochs/2.6.9/${BOCHS}.tar.gz && \
            echo "[Info]: downloads ${BOCHS}.tar.gz Sucessfully." || (rm ${BOCHS}.tar.gz & echo "[Warning]: downloads ${BOCHS}.tar.gz unsuccessfully !!!" )
        fi
        echo "[Info]: tar ${BOCHS}.tar.gz..."
        tar zxvf ${BOCHS}.tar.gz &> /dev/null && \
        echo "[Info]: tar ${BOCHS}.tar.gz Sucessfully." || echo "[Warning]: tar ${BOCHS}.tar.gz unsuccessfully !!!"
        cd ..
    fi

    if [ -d "${DIR}/${BOCHS}" ];then
        cd ${DIR}/${BOCHS}
        ./configure --enable-debugger --enable-disasm &&\
        make
    fi

}

env_install(){
    GCC_DIR="gcc-3.4"
    INSTALL_LIST=("make" "bin86" "gcc-multilib")
    DOWNLOAD_LIST=("gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb" "gcc-3.4_3.4.6-6ubuntu3_amd64.deb" "cpp-3.4_3.4.6-6ubuntu3_amd64.deb" \
        "g++-3.4_3.4.6-6ubuntu3_amd64.deb" "libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb")

    for i in ${INSTALL_LIST[*]}; do 
        ( dpkg -l |grep ${i} &> /dev/null || (echo "[Info]: installing ${i} ..." & sudo apt-get install ${i}))    \
        && echo "[Info]: ${i} is installed." || echo "[Error]: ${i} is not installed."
    done

    if [ -z `which gcc-3.4` ]; then
        echo "[Info]: Start installing gcc-3.4..."
        for deb in ${DOWNLOAD_LIST[*]}; do
            if [ ! -e ${GCC_DIR}/${deb} ]; then
                echo "[Info]: downloading ${deb} ..."
                wget -qO- http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/${deb} -P ${GCC_DIR} && \
                echo "[Info]: download ${deb} Sucessfully." || ( rm ${deb} & echo "[Warning]: download ${deb} unsuccessfully !!!" )
            fi
        done
        sudo dpkg -i ${GCC_DIR}/*.deb &> /dev/null
        sudo apt-get install -f &> /dev/null
        if [ ! -z `which gcc-3.4` ];then
            echo "[Info]: gcc-3.4 is installed."
        fi
    fi
}

trap 'onCtrlC' INT
function onCtrlC () {
    echo "[Warning]: stopped by user."
    rm ${GCC_DIR}/*.deb
    exit
}

if [ "$1" ] && [ "$1" = "-b" ]
then
    bochs_install
elif [ "$1" ] && [ "$1" = "-bm" ]
then
    bochs_make_install
elif [ "$1" ] && [ "$1" = "-e" ]
then
    env_install
else
    env_install
    bochs_install
fi