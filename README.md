## Overview
This is a cross-platform ROM streaming program that sends data to another device via a serial connection.

## Programmer
This program is meant to be paired with some kind of external microcontroller or CPU.
I've included an Arduino sketch that writes the streamed data to an '28CXXX EEPROM in the avr folder of this repo if you would like to pair it as well.

I'm using a completely arbitrary and totally made up uploading protocol.

## Protocol
Documentation for this protocol is included in the "protocol" file in the root of the repo.
Using this documentation should allow it to be ported to any system relatively easily.
