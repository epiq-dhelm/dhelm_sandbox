#!/bin/bash

PS4='Line ${LINENO}: '
set -x
set -e

ubuntu_version=2204

sidekiq_version=4.17.6

kernel_version=5.19.0-43-generic


source ./arg_parser.sh
parse_commandline "$@"

echo "ubuntu_version:" $ubuntu_version
echo "library_version:" $sidekiq_version
echo "kernel_version:" $kernel_version

cd ~/sidekiq_release
git pull
git subm

cd drivers/docker/ubuntu-kernel-modules-$ubuntu_version
pwd

docker build -t docker.epiq.rocks/ubuntu-kernel-modules-$ubuntu_version:$sidekiq_version .

echo "display docker libraries"
docker run --name myname -ti -d -u ${USER_ID}:${GROUP_ID}   docker.epiq.rocks/ubuntu-kernel-modules-$ubuntu_version:$sidekiq_version

docker ps -a


docker exec myname pwd
docker exec myname ls /lib/modules

docker stop myname
docker container rm myname

echo "make the drivers"
cd ../../sidekiq_driver_release
echo """$kernel_version""" | xargs ./scripts/build-modules-with-docker.sh -d docker.epiq.rocks/ubuntu-kernel-modules-$ubuntu_version:$sidekiq_version

ls output/x86-ubuntu/$kernel_version

cd ../sidekiq_gps_release
echo """$kernel_version""" | xargs ./scripts/build-modules-with-docker.sh -d docker.epiq.rocks/ubuntu-kernel-modules-$ubuntu_version:$sidekiq_version

ls output/x86-ubuntu/$kernel_version
cp output/x86-ubuntu/$kernel_version/sidekiq_gps.ko ../sidekiq_driver_release/output/x86-ubuntu/$kernel_version/.

cd ../sidekiq_uart_release
echo """$kernel_version""" | xargs ./scripts/build-modules-with-docker.sh -d docker.epiq.rocks/ubuntu-kernel-modules-$ubuntu_version:$sidekiq_version

ls output/x86-ubuntu/$kernel_version
cp output/x86-ubuntu/$kernel_version/sidekiq_uart.ko ../sidekiq_driver_release/output/x86-ubuntu/$kernel_version/.

cd ../sidekiq_driver_release/output/x86-ubuntu
ls $kernel_version

tar -czvf linux_kernel-$kernel_version-for-libsidekiq_v$sidekiq_version.tar.xz  $kernel_version

ls -la




