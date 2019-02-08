FROM ubuntu:latest

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
    && apt-get autoclean \
    && locale-gen en_US.UTF-8

# install xfce
RUN apt-get update \ 
	&& apt-get install -y supervisor \
	xfce4 \
	xfce4-terminal

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
    make bin86 gcc-multilib

RUN wget -q --show-progress http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb && dpkg -i gcc-3.4-base_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/gcc-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i gcc-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/cpp-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i cpp-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/g++-3.4_3.4.6-6ubuntu3_amd64.deb && dpkg -i g++-3.4_3.4.6-6ubuntu3_amd64.deb || echo
RUN wget -q --show-progress http://old-releases.ubuntu.com/ubuntu/pool/universe/g/gcc-3.4/libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb && dpkg -i libstdc++6-dev_3.4.6-6ubuntu3_amd64.deb || echo
RUN apt-get install -y -f && rm *.deb

ADD ./vnc_startup.sh ${HOME}
RUN chmod +x ${HOME}/vnc_startup.sh

USER ${USER}
WORKDIR ${HOME}

# install novnc for ${USER} and change vncpassword
RUN mkdir -p ${NO_VNC_HOME}/utils/websockify && cd ${NO_VNC_HOME}/utils
RUN wget -qO- https://github.com/novnc/noVNC/archive/v1.0.0.tar.gz | tar xz --strip 1 -C ${NO_VNC_HOME}
RUN wget -qO- https://github.com/novnc/websockify/archive/v0.6.1.tar.gz | tar xz --strip 1 -C ${NO_VNC_HOME}/utils/websockify
RUN chmod +x -v $NO_VNC_HOME/utils/*.sh
RUN (echo $NO_VNC_PASSWORD; echo $NO_VNC_PASSWORD; echo n) | vncpasswd

EXPOSE 6080