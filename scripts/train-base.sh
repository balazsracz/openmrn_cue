#!/bin/bash

export LOC=localhost:8083

function calltrain() {
  (set -x ; stubby --proto2 --security_protocol="" --rpc_security_protocol=""  --protofiles /home/bracz/tgit/google3/experimental/users/bracz/train/host/train_control.proto call "$LOC" "TrainControlService.TrainControl" "$@" )
}

function rcalltrain() {
  echo rcalltrain "$@"
  until calltrain --deadline 0.5 "$@" | grep 'success: true' ; do
    echo retrying...
  done
}

# args: $1: train id. $2: accessory id (1=speed 2=light) $3: value
function accessory() {
  id=$(($1 * 4 + 1))
  ofs=$2
  value=$3
#  calltrain  "DoSendRawCanPacket { wait:true d:0x40 d:0x48 d:$id d:0x01 d:3 d:$ofs d:0 d:$value } "
  calltrain  "DoSetAccessory { train_id: $1 accessory_id: $2 value:$3 } "
}

function log() {
  echo $(date) $(id -un) $(hostname) "$@" >> ~bracz/trlog
}

function getaccessory() {
  id=$(($1 * 4 + 1))
  ofs=$2
  calltrain  "DoSendRawCanPacket { wait:true d:0x40 d:0x48 d:$id d:0x01 d:2 d:$ofs d:0 } "
}

log "$0" "$@"

function sendpkt() {
  echo sending OpenLCB packet "$1"
  echo "$1" >> /dev/tcp/localhost/12021
 # echo "$1" >> /usr/local/google/bracz/train/openlcb
}
