#!/bin/bash

source $(dirname $0)/train-base.sh

export DST=$1
if [ ${#DST} == 2 ]; then
    DST=0x0501010114$DST
fi
train-send-datagram -n $DST  -g 20A9
