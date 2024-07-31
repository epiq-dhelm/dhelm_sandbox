#!/bin/bash

#cp my_docker-compose.yml docker-compose.yml

cd pytest/framework/

# puts in my nuc nodes
cp my_nuc_info.json node_info.json

# only copies the necessary files to target
#cp my_framework.py framework.py

cd ../..

docker image rm -f sidekiq_sw-framework

docker build --platform linux -f Dockerfile.pytest -t docker.epiq.rocks/pytest:dev-$(id -un) --build-arg USER=$(id -un) --build-arg USER_UID=$(id -u) --push .

USER=dhelm REMOTE_USER=dave USER_UID=$(id -u)  LOCAL_DIR=$(pwd) HOST=192.168.2.151 docker compose up framework
