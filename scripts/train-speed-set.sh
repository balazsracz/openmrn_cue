#!/bin/bash

source /home/bracz/bin/train-base.sh

#args: $0 address speed
add=${1:-22}
spd=${2:-15}

sendpkt $(printf ":X195B4001N0501010114FE%02x%02x;" $add $spd) 
