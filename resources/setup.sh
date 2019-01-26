#/bin/bash

sudo echo "Start installing gcc-3.4"

trap 'onCtrlC' INT
function onCtrlC () {
    echo '[Warning]: stopped by user'
    rm *.deb
    exit
}

DOWNLOAD_LIST=("gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb" "gcc-3.4_3.4.6-6ubuntu3_amd64.deb" "cpp-3.4_3.4.6-6ubuntu3_amd64.deb" \
    "g++-3.4_3.4.6-6ubuntu3_amd64.deb" "libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb")

if [ -z `which gcc-3.4` ]; then
    for deb in ${DOWNLOAD_LIST[*]}; do
        if [ ! -e deb ]; then
            echo "[Info]: downloading ${deb} ..."
            wget http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/${deb} &> /dev/null && \
            echo "[Info]: download ${deb} Sucessfully." || ( rm ${deb} & echo "[Warning]: download ${deb} unsuccessfully !!!" )
        fi
        sudo dpkg -i ${deb} &> /dev/null
    done
fi

sudo apt-get install -f && echo "[Info]: gcc-3.4 is installed Sucessfully"

echo "Start install as86 ld86"

sudo apt-get install bin86 && echo "[Info]: bin86 is installed Sucessfully"