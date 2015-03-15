#!/bin/bash

# usage: $0 controller-address

if [ -z "$1" ] || [ -z "$2" ] ; then
  echo usage: $0 controller-address-base packet
  echo example $0 4D 00030301
  exit 1
fi

train-send-datagram -n 0501010114$1 -g 2F10$2
