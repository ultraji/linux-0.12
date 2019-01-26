#/bin/bash

sudo echo "Start installing gcc-3.4"

DOWNLOAD_LIST=("gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb" "gcc-3.4_3.4.6-6ubuntu3_amd64.deb" "cpp-3.4_3.4.6-6ubuntu3_amd64.deb" \
    "g++-3.4_3.4.6-6ubuntu3_amd64.deb" "libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb")

for deb in ${DOWNLOAD_LIST[*]}; do
    if [ ! -e deb ]; then
        wget http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/${deb} > /dev/null && \
        echo "[Info]: download ${deb} Sucessfully." || echo "[Warning]: download ${deb} unsuccessfully !!!"
        sudo dpkg -i ${deb}
    fi
done

sudo apt-get install -f && echo "[Info]: gcc-3.4 is installed Sucessfully"