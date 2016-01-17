#!/bin/bash

source $(dirname $0)/train-base.sh

if [ -z "$1" ] ; then
  echo usage $0 '<alias>'
  echo alias is 3 hex digits
  exit 1
fi

sendpkt ":X19968575N0$1;"
