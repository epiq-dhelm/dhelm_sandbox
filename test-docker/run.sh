#!/bin/bash

# Run the Docker container
docker run -it -d --name mytest$1 --privileged --ipc=host test.$1

# go into the container with a bash shell
docker exec -it mytest$1 /bin/bash

