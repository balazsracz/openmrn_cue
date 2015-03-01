#!/bin/bash

# usage: $0 controller-address signal-address


if [ -z "$1" ] ; then
  echo usage: $0 controller-address-base [signaladdress]
  echo example $0 4D 00
  exit 1
fi

SIGADDR=${2:-00}

train-send-datagram -n 0501010114$1 -g 2F10${SIGADDR}030155
