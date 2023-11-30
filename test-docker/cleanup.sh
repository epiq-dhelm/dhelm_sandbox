#!/bin/bash

docker stop mytest$1
docker rm mytest$1

docker image rm -f test.$1

