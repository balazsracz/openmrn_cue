#!/bin/bash

# usage: $0 controller-address

if [ -z "$1" ] ; then
  echo usage: $0 controller-address-base
  echo example $0 4D
  exit 1
fi

train-send-datagram -n 0501010114$1 -g 2F18
