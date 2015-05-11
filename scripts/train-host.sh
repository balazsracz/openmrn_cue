#!/bin/bash

DEVICE=${DEVICE:-/dev/trainhost_ti}

PARAMS=" --nobinarylog --noenable_overcurrent_shutdown  --packet_payload_size=10 --gid= --uid= --loas_pwd_fallback_in_corp --rpc_security_protocol= --use_pic_for_can --fake_lokdb=  --logtostderr --picusb_device=${DEVICE} "

#PARAMS+=" --openlcb_hub_host=localhost --openlcb_hub_port=12021 "
PARAMS=" -n 0x050101011432"

PARAMS+=" -l /home/bracz/bin/train-lokdb"

PARAMS+=" -R localhost -P 20001"
#PARAMS+=" -R 28k.ch -P 50001"
PARAMS+=" -d ${DEVICE}"


while true ; do 
    while [ ! -c $DEVICE ] ; do
        echo sleeping
        sleep 0.1
    done
    ~/bin/train-nhost $PARAMS "$@" 2>&1 | tee /tmp/trainlog
done
