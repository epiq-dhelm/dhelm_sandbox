#!/bin/bash
  
PS4='Line ${LINENO}: '
set -x


while read -r line;
do
    echo "stopping PID $line"
    kill -KILL $line

    sleep 3
done < pid.txt


