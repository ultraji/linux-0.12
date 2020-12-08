FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i "s/archive.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/g"  /etc/apt/sources.list

# install tools
RUN apt-get update && apt-get install -y \
    vim \
    wget \ 
    net-tools \
    locales \
    python-numpy \
    git \
    sudo \
    gdb \
    ttf-wqy-zenhei \
    && locale-gen en_US.UTF-8 \
    && apt-get autoclean && rm -rf /var/lib/apt/lists/*

# install xfce
RUN apt-get update \ 
	&& apt-get install -y \
    supervisor \
	xfce4 \
	xfce4-terminal \
    && apt-get autoclean && rm -rf /var/lib/apt/lists/*

# install tigervnc
RUN wget -qO- https://dl.bintray.com/tigervnc/stable/tigervnc-1.8.0.x86_64.tar.gz | tar xz --strip 1 -C /

ENV LANG='en_US.UTF-8' \
    LANGUAGE='en_US:en' \
    LC_ALL='en_US.UTF-8' \
    USER=ubuntu \
    PASSWORD=123456 \
    HOME=/home/ubuntu \
    NO_VNC_HOME=/home/ubuntu/.novnc \
    NO_VNC_PASSWORD=123456

# change the password of root and add user
RUN (echo "root:123456" | chpasswd) && adduser ${USER} --disabled-password && (echo "${USER}:${PASSWORD}" | chpasswd) && usermod -aG sudo ${USER}

RUN apt-get update && apt-get install -y \
    build-essential bochs bochs-x bochs-sdl \
    make bin86 gcc-multilib \
    && apt-get autoclean && rm -rf /var/lib/apt/lists/*

RUN wget -q --show-progress http://mirrors.ustc.edu.cn/ubuntu-old-releases/ubuntu/pool/universe/g/gcc-3.4/gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb && dpkg -i gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://mirrors.ustc.edu.cn/ubuntu-old-releases/ubuntu/pool/universe/g/gcc-3.4/gcc-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i gcc-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://mirrors.ustc.edu.cn/ubuntu-old-releases/ubuntu/pool/universe/g/gcc-3.4/cpp-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i cpp-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://mirrors.ustc.edu.cn/ubuntu-old-releases/ubuntu/pool/universe/g/gcc-3.4/g++-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i g++-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://mirrors.ustc.edu.cn/ubuntu-old-releases/ubuntu/pool/universe/g/gcc-3.4/libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb && dpkg -i libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb || echo
RUN apt-get install -y -f && rm *.deb && apt-get autoclean && rm -rf /var/lib/apt/lists/*

ADD conf/vnc_startup.sh ${HOME}
RUN chmod +x ${HOME}/vnc_startup.sh

USER ${USER}
WORKDIR ${HOME}

# install novnc for ${USER} and change vncpassword
RUN mkdir -p ${NO_VNC_HOME}/utils/websockify && cd ${NO_VNC_HOME}/utils
RUN wget -qO- https://github.com/novnc/noVNC/archive/v1.1.0.tar.gz | tar xz --strip 1 -C ${NO_VNC_HOME}
RUN wget -qO- https://github.com/novnc/websockify/archive/v0.9.0.tar.gz | tar xz --strip 1 -C ${NO_VNC_HOME}/utils/websockify
RUN chmod +x -v $NO_VNC_HOME/utils/*.sh
RUN (echo $NO_VNC_PASSWORD; echo $NO_VNC_PASSWORD; echo n) | vncpasswd
RUN cp ${NO_VNC_HOME}/vnc.html ${NO_VNC_HOME}/index.html

EXPOSE 6080

CMD ["./vnc_startup.sh"]