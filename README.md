## Overview
This is a cross-platform ROM streaming program that sends data to another device via a serial connection, that is then flashed to an SRAM or EEPROM chip for prototyping.

## Why this was needed
Working on projects that require ROMs can be painful during the prototyping phase. 
Compiling, burning, removing chip from burner, inserting it into the breadboard or socket, testing, removing, repeat.

I ended up damaging an EEPROM doing this cycle too many times and no realizing I didn't remove power and misplaced the chip on my breadboard.

With this uploader, paired with an arudino, I've been able to flash my "ROM" data into an SRAM chip and then boot the system from there.
This has DRASTICALLY cut down on my prototyping time and allows me to make changes in my code and re-upload in seconds.


## Programmer
This program is meant to be paired with some kind of external microcontroller. I have been using an arduino nano for my build.
I've included an Arduino sketch in the avr folder of this repo if you would like to pair it as well.

I'm using a completely arbitrary and totally made up uploading protocol, which as of now is not documented in this repo.
I hope to document it soon so it is easier to port to other microcontrollers besides an arduino.



Note: I am trying to eventually port the Arduino sketch to plain C because I'm not a fan of the arduino IDE or the overhead of its various built-in functions
but it was the faster choice for now because I wanted this working ASAP.