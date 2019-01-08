#!/bin/bash

source /home/bracz/bin/train-base.sh

#args: $1 is a train address (up to 255)
# $2 is the desired speed

sendpkt $(printf ":X195B4001N0501010114FE%02x%02x;" $1 $2) 
