#!/bin/bash

exec 4<> /dev/tcp/localhost/12021

while true; do
    echo ":X195B4002N09000DFF0000000B;" >&4
    sleep 0.02
done
