#!/bin/bash

PS4='Line ${LINENO}: '
set -x
set -e


export UBUNTU_VERSION=$1

# Build the Docker image
docker build -f Dockerfile --build-arg UBUNTU_VERSION=$1 -t  test.$1 .


# Run the Docker container
docker run -it -d --name mytest$1 --privileged --ipc=host test.$1
docker cp install_current.sh mytest$1:/home/sidekiq/.

# go into the container with a bash shell
docker exec -it mytest$1 /bin/bash
