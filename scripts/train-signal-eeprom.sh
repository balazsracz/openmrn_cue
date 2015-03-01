#!/bin/bash

# usage: $0 controller-address

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] || [ -z "$4" ]; then
  echo usage: $0 controller-address-base signal-address eepromaddress value
  echo example $0 4D 00 00 1F
  exit 1
fi

train-send-datagram -n 0501010114$1 -g 2F10$(printf %02x $2)0402$(printf %02x $3)$(printf %02x $4)
