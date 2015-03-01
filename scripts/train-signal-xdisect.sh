#!/bin/bash

# usage: $0 controller-address

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] ; then
  echo usage: $0 controller-address-base low hi
  echo example $0 4D 0 126
  exit 1
fi

export A=$1

LO=$2
HI=$3

train signal-pause $A

function aspect() {
  train signal-packet $A $(printf %02x $1)0303$(printf %02x $2)
}

function reinit() {
  train signal-packet $A $(printf %02x $1)030155
}


reinint

while [ $LO -lt $HI ] ; do 
  reinit

  sleep 0.5

  MI=$((($LO + $HI) / 2))

  echo trying $LO to $MI from [$LO, $HI]

  DGOPTS=
  for i in $(seq $LO $MI) ; do
    DGOPTS+=" -g 2F10$(printf %02x $i)0209"
  done

  train-send-datagram -n 0x0501010114$A  $DGOPTS
  
  select resp in found not-found ; do
    case $resp in
      found) HI=$MI
        ;;
      not-found) LO=$(($MI + 1))
        ;;
    esac
    break;
  done

done

echo lo $LO hi $HI  hex 0x$(printf %02x $LO)

aspect $LO 5
