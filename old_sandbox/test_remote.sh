#!/bin/bash

set -x

DIR="/tmp/regression"
REMOTE="sidekiq@10.42.0.155"

HASH=$(echo "$REMOTE" | openssl enc -base64 | tr -d '=')
SSH="ssh -o ControlPath=$HASH.socket -o ControlMaster=auto -o ControlPersist=600"


$SSH -t "$REMOTE" "rm -rf ${DIR:?}/* &&
                   [ ! -d $DIR ] &&
                   echo 'directory $DIR does not exist, creating' &&
                   mkdir -p $DIR"

echo "done"

