#!/bin/bash

PS4='Line ${LINENO}: '
set -x
set -e

os_version=22.04

sidekiq_version=4.17.6

kernel_version=5.19.0-83-generic


source ./arg_parser.sh
parse_commandline "$@"

echo "os_version:" $os_version
echo "library_version:" $sidekiq_version
echo "kernel_version:" $kernel_version

# don't checkout the release, go with the latest.
cd ~/sidekiq_release
git checkout master
git pull
git subm

echo "see if container running"

if [ "$( docker container inspect -f '{{.State.Running}}' ubuname )" == "true" ]
then 
    docker stop ubuname
    docker container rm ubuname
fi

docker run --name ubuname  -ti -d -v "$(pwd)":/home/dhelm/sidekiq_release ubuntu:$os_version /bin/bash


docker exec ubuname apt update
docker exec ubuname apt install -y build-essential kmod gcc-12
docker exec ubuname apt install -y linux-headers-$kernel_version
docker exec ubuname ls /lib/modules


docker exec -i ubuname bash -c "cd /home/dhelm/sidekiq_release && scripts/compile_drivers.sh $kernel_version"
docker exec -i ubuname bash -c "cd /home/dhelm/sidekiq_release && scripts/bundle_drivers.sh $kernel_version"
docker exec -i ubuname bash -c "cd /home/dhelm/sidekiq_release && ls -la"

docker stop ubuname
docker container rm ubuname

cp linux_kernel-$kernel_version-for-.tar.xz /home/dhelm/dhelm_sandbox/driver-docker/linux-kernel-$kernel_version-for-libsidekiq_v$sidekiq_version.tar.xz

