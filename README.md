openmrn_cue
===========

An application with additional modules for OpenMRN.

OpenMRN is a generic, hardware- and OS-independent implementation of the OpenLCB (aka NMRAnet) standard for controlling model railroads. There are core modules and basic ports committed to github:bakerstu/openmrn (ask bakerstu for acces).

This repository extends those core modules with a number of hardware- and application-specific modules that built together make into a usable model railroad control system.

Modules include:
- A command station producing track packet stream for DCC + MM protocols.
- Simple event producers and consumers for GPIO ports, to be meant for controlling turnout and sensing trains.
- An I2C extender module that allows slave boards connected to an openMRN node for port count expansion.
- A watchdog that resets tha chip if it detects certain threads blocked indefinitely.
- A "cue node" which allows layout control logic to be executed in the node. An example application of a RR-crossing sign (if train is here -> flash two lights alternating).
- IO modules for interfacing via GridConnect protocol.
- Code for interfacing to a Marklin mobile station 1 as a throttle.

Some of these modules will be back-ported to the main OpenMRN tree.

Hardware support:
- linux

Various NXP chips with ARM cores:
- lpcxpresso LPC11C24 dev board
- mbed 1768 board. Consider using the TCH technology baseboard for it.
- FEZ Panda2 (with LPC2368), reflashed to be a pure ARM devboard.

