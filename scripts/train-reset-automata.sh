#!/bin/bash

source $(dirname $0)/train-base.sh

export DST=0x050101011432

train-send-datagram -n $DST  -g F010
sleep 1
train-send-datagram -n $DST  -g F011
