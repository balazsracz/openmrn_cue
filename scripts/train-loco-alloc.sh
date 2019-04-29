#!/bin/bash

source $(dirname $0)/train-base.sh

NUM=${1:-1}
for adr in $(seq 300 $((300 - 1 + $NUM))) ; do
    sendpkt ":X19914575N090099FFFF$(printf %04d $adr)C0;"
    sleep 0.2
done

