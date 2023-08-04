#!/bin/bash
set -e
set -x

sudo cp /etc/fstab.mount /etc/fstab

sudo mount /mnt/storage


sudo cp /etc/fstab.default /etc/fstab
