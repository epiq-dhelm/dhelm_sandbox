#!/bin/sh
LIST_OF_APPS="apt-utils git cmake g++ libboost-all-dev libgmp-dev swig python3-numpy 
python3-mako python3-sphinx python3-lxml doxygen libfftw3-dev 
libsdl1.2-dev libgsl-dev libqwt-qt5-dev libqt5opengl5-dev python3-pyqt5 
liblog4cpp5-dev libzmq3-dev python3-yaml python3-click python3-click-plugins 
python3-zmq python3-scipy python3-gi python3-gi-cairo gir1.2-gtk-3.0 
libcodec2-dev libgsm1-dev libusb-1.0-0 libusb-1.0-0-dev libudev-dev 
pybind11-dev python3-matplotlib libsndfile1-dev python3-pip libsoapysdr-dev soapysdr-tools 
 libiio-dev libad9361-dev libspdlog-dev python3-packaging python3-jsonschema"

apt-get update -y
apt-get install $LIST_OF_APPS -y
#pip install pyqtgraph pygccxml -y

virtualenv -q -p /usr/bin/python3.5 $1
$1/bin/pip install pyqtgraph pygccxml 

add-apt-repository ppa:gnuradio/gnuradio-releases -y
apt-get update -y
apt-get install gnuradio python3-packaging -y

apt-get install clang-format -y
#cd /home/recorderuser/gr-sidekiq-new_overruns
#chmod +x ./bind.sh ./build_install.sh
#./bind.sh
#./build_install.sh
apt-get install xterm -y

#automount drives 
#reformat drives??



