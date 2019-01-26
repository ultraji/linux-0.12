#/bin/sh

BOCHS="bochs-2.6.9"

sudo echo "Start installing ${BOCHS}"

trap 'onCtrlC' INT
function onCtrlC () {
    echo '[Warning]: stopped by user'
    rm ${BOCHS}.tar.gz
    rm -rf ${BOCHS}
    exit
}

if [ ! -d "${BOCHS}" ];then
    if [ ! -e "${BOCHS}.tar.gz" ]; then
        echo "[Info]: downloading ${BOCHS}.tar.gz..."
        wget https://downloads.sourceforge.net/project/bochs/bochs/2.6.9/${BOCHS}.tar.gz &> /dev/null && \
        echo "[Info]: downloads ${BOCHS}.tar.gz Sucessfully." || (rm ${BOCHS}.tar.gz & echo "[Warning]: downloads ${BOCHS}.tar.gz unsuccessfully !!!" )
    fi
    echo "[Info]: tar ${BOCHS}.tar.gz..."
    sudo tar zxvf ${BOCHS}.tar.gz &> /de/null && \
    echo "[Info]: tar ${BOCHS}.tar.gz Sucessfully." || echo "[Warning]: tar ${BOCHS}.tar.gz unsuccessfully !!!"
fi