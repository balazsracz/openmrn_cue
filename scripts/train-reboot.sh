#!/bin/bash

source $(dirname $0)/train-base.sh

export DST=$1

train-send-datagram -n 0x0501010114$DST  -g 20A9
