FROM ubuntu:20.04

RUN apt update && apt-get install apt-utils software-properties-common sudo xz-utils udev kmod -y

RUN apt-get install python3-pip git -y

RUN apt update && apt install -y openssh-server

RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

EXPOSE 22

RUN useradd -ms /bin/bash recorderuser

RUN echo "recorderuser:record" | chpasswd

RUN echo '%recorderuser ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER recorderuser

WORKDIR /home/recorderuser

COPY src/  /home/recorderuser

RUN sudo /home/recorderuser/install_sidekiq_complete_for_v4.17.6-26-g5517cd98432c_20230605_x86.sh

RUN sudo /home/recorderuser/install_grc.sh

RUN sudo apt-get install dbus dbus-x11 -y

RUN git clone https://github.com/epiqsolutions/gr-sidekiq.git

WORKDIR /home/recorderuser/gr-sidekiq

RUN ./build_install.sh


