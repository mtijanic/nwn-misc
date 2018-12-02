#!/bin/bash

status=0

while read -r line
do
    if output=$(ps -p "$line" >/dev/null 2>&1)
    then
        status=1
    fi
done <  ~/.modpid

echo $status
