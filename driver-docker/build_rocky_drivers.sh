#!/bin/bash

PS4='Line ${LINENO}: '
set -x
set -e

os_version=9.3

sidekiq_version=4.17.6

kernel_version=4.18.0-477.15.1.el8_8.x86_64



# the ending is different per kernel, so look it up here https://download.rockylinux.org/pub/rocky/8/Devel/x86_64/os/Packages/k/
kernel_version_ending=el9_3

long_kernel_version=$kernel_version_ending.x86_64

source ./arg_parser.sh
parse_commandline "$@"

echo "os_version:" $os_version
echo "library_version:" $sidekiq_version
echo "kernel_version:" $kernel_version
echo "kerner_version_ending:" $kernel_version_ending
echo "long_kernel_version:" $long_kernel_version

cd ~/sidekiq_release
git checkout master
git pull
git subm
#git checkout sidekiq_release_v$sidekiq_version

echo "see if container running"
if [ "$( docker container inspect -f '{{.State.Running}}' rockyname )" == "true" ]
then
    docker stop rockyname
    docker container rm rockyname
fi

docker pull rockylinux/rockylinux:$os_version
docker run --name rockyname -ti -d  -v "$(pwd)":/home/dhelm/sidekiq_release rockylinux:$os_version

docker exec rockyname yum update -y

docker exec rockyname yum install -y  sudo
docker exec rockyname sudo yum groupinstall -y "Development Tools"

docker exec rockyname yum install -y kernel-devel-$kernel_version.$kernel_version_ending

docker exec rockyname ls /usr/src/kernels/

# the compile_drivers.sh and bundle_drivers.sh expect the kernel headers to be in /lib/modules
# But on rocky linux it is loced in /usr/src/kernels/  
# So create a symbolic link between the two.
docker exec -i rockyname bash -c "cd /lib/modules && ln -s /usr/src/kernels/$kernel_version.$long_kernel_version $kernel_version.$long_kernel_version"

# run compile drivers
docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && scripts/compile_drivers.sh $kernel_version.$long_kernel_version"

# run bundle drivers
docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && scripts/bundle_drivers.sh $kernel_version.$long_kernel_version"
docker exec -i rockyname bash -c "cd /home/dhelm/sidekiq_release && ls -la"

docker stop rockyname
docker container rm rockyname

# move and change the name of the result file.
cp linux_kernel-$kernel_version.$long_kernel_version-for-.tar.xz /home/dhelm/dhelm_sandbox/driver-docker/linux-kernel-$kernel_version.$long_kernel_version-for-libsidekiq_v$sidekiq_version.tar.xz



