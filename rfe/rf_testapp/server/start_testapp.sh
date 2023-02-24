#!/bin/bash
  
PS4='Line ${LINENO}: '
set -x

rm -f pid.txt

while read -r line;
do
    echo "starting card serial number $line";
    bin/x86_64.gcc/testapp_server --server-addr 127.0.0.1 --port $line -S $line &
    last_pid=$!

    echo $last_pid
    echo $last_pid >> pid.txt
    sleep 3
done < input.txt


