#!/bin/bash

source $(dirname $0)/train-base.sh

export DST=$1

sendpkt ":X1A${DST}575N20AB;"
