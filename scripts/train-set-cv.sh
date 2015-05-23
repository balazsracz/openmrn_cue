#!/bin/bash

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] ; then
  echo usage: $0 dcc-address cv-number value
  echo example $0 3 19 35
  exit 1
fi

train-send-datagram -n 0x06010000$(printf %04x $1) -g 2000000000$(printf %02x $(($2 - 1)))EF$(printf %02x $3) -w
