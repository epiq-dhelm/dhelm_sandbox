#!/bin/bash

PS4='Line ${LINENO}: '
set -x
set -e

os_version=8

sidekiq_version=4.17.6

kernel_version=4.18.0-477.15.1.el8_8.x86_64


source ./arg_parser.sh
parse_commandline "$@"

echo "os_version:" $os_version
echo "library_version:" $sidekiq_version
echo "kernel_version:" $kernel_version

cd ~/sidekiq_release
git checkout master
git pull
git subm
git checkout sidekiq_release_v$sidekiq_version

docker pull rockylinux/rockylinux:$os_version
docker run --name rockyname -ti -d  -v "$(pwd)":/home/dhelm/sidekiq_release rockylinux:$os_version

docker exec rockyname yum update -y

docker exec rockyname yum install -y  sudo
docker exec rockyname sudo yum groupinstall -y "Development Tools"
docker exec rockyname yum install -y kernel-devel-$kernel_version

docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && scripts/compile_drivers.sh $kernel_version"
docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && scripts/bundle_drivers.sh $kernel_version"
docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && ls -la"

docker stop rockyname
docker container rm rockyname

cp linux_kernel-$kernel_version-for-.tar.xz /home/dhelm/driver-docker/linux-kernel-$kernel_version-for-libsidekiq_v$sidekiq_version.tar.xz



