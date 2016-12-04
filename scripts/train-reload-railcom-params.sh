#!/bin/bash

cd ~/train/openlcb/gitsvn/svn

for i in 2 3 4 5 6 ; do
  java -cp openlcb.jar org.openlcb.cdi.cmd.RestoreConfig 05.01.01.01.14.E$i localhost 12021 05.01.01.01.14.9$i ~/train/openlcb/private_openmrn/automata/config.railcom.params
#PIDS=$PIDS $!
done

wait $PIDS
