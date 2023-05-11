## Purpose
To stream raw ROM data to an arduino that is then flashed to an SRAM or EEPROM chip for prototyping.

## Why this was needed
I have been working on a z80 SBC on and off (mostly off lol) for a few years.
I didn't want to buy an EEPROM programmer so I just use an arduino to program it for me, though it is probably a lot slower that way.
The ROM data would be burned into the arduino's flash in byte form and written to the EEPROM.

When I first started working on the project my workflow was like this:

Write z80 assembly -> assemble it with z80asm -> convert the bin into intel hex -> convert intel hex into 0x00 format for the arduino byte array ->
flash the arduino with the new ROM data ->remove EEPROM from breadboard -> connect EEPROM to arduino's breadboard -> have the arduino flash the EEPROM -> 
remove EEPROM and replace back into z80's breadboard -> reset z80 and test


I am VERY bad at z80 ASM (and I'm new to programming in general) so I was doing the above workflow a LOT because I was having to make tons of small
changes and tweaks while I learned.


Eventually I replaced the EEPROM on the z80's breadboard with another SRAM chip and tossed in a few 2n2222's to allow the arduino to hold the z80
in reset mode, isolate the SRAM from the z80, program the SRAM, and then relinquish control to the z80 and then let go of the reset.

As far as the z80 is concerned the SRAM is treated as ROM and worked great for prototyping, as long as the power stayed on!
The arduino was now integrated into the z80's breadboard and I was able to control it separately with its serial interface.

BUT the work flow was still cumbersome because I had to reflash the new ROM data to the arduino when I wanted to try out some changes to my code.


The solution? I decided to write a computer program that will stream the z80 hex/bin data to the arduino and have it then program the data into 
the SRAM chip. This would allow VERY rapid changes being implemented quickly and without eating away at the atmega328p's erase count.


## Arduino
This program is meant to be paired with my Arduino sketch that is included in the repo under the avr folder.


I am currently in the process of porting this to plain C because I'm not a fan of the arduino IDE or the overhead of its various built-in functions
but it was the faster choice because I wanted this working ASAP.
