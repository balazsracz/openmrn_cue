#!/bin/bash

source $(dirname $0)/train-base.sh

exec 4>>/dev/tcp/localhost/12021

NUM=${1:-1}
START=${2:-0}
for adr in $(seq $((300 + $START)) $((300 - 1 + $NUM))) ; do
    sendpkt4 ":X19914575N090099FFFF$(printf %04d $adr)C0;"
    sleep 0.2
done

