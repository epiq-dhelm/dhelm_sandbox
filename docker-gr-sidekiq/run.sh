#!/usr/bin/env bash


xhost +local:docker

docker run \
       -ti \
       --rm \
       --name gr-sidekiq \
       -e DISPLAY=unix$DISPLAY \
       -v /tmp/.X11-unix/:/tmp/.X11-unix/ \
       -v $HOME/.Xauthority:/home/dhelm/.Xauthority \
       --ipc=host \
       --net=host \
       --privileged \
       gnuradio \
       /bin/bash 
