#!/bin/bash

source $(dirname $0)/train-base.sh

sendpkt ":X198F4575N0501010114FF$1;"
sendpkt ":X19914575N0501010114FF$1;"
