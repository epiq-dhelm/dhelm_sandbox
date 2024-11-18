FROM rockylinux/rockylinux:9.3

RUN yum update -y

RUN yum install -y  sudo

RUN sudo yum groupinstall -y "Development Tools"
RUN sudo yum install libtirpc.x86_64



RUN useradd -ms /bin/bash recorderuser

RUN echo "recorderuser:record" | chpasswd

RUN echo '%recorderuser ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER recorderuser

WORKDIR /home/recorderuser

# need to make sure the complete installer is renamed install_current.sh and put into src directory

COPY src/  /home/recorderuser

#RUN sudo /home/recorderuser/install_current.sh





