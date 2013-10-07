avrpp
=====

This repository contains some C++ utility libraries for AVR microcontrollers,
as well as a few projects using these libraries.

In particular, it contains the following generic library code:

*   USB utility code for building USB devices.

*   A debug logging framework, and a USB debug interface compatible with PJRC's
    [`hid_listen` program](http://www.pjrc.com/teensy/hid_listen.html)

    This debug interface also supports SET_REPORT calls to reboot the device.

*   A generic keyboard controller implementation, along with a USB keyboard
    interface.

*   A bit-banging I2C implementation, as well as some basic code for
    interacting with an 24c08 EEPROM over an I2C bus.  This can be used for
    performing I2C communication over generic I/O pins rather than than the
    hardware-supported pins.

This repository also contains full keyboard controller implementations
for two different physical keyboard schematics that I currently have.
(These are for two different generations of Maltron dual-handed keyboards,
where I replaced the factory controller with my own implementation.)


Acknowledgements
================

I have been primarily been developing these projects using the
[Teensy 2.0++ USB board](http://www.pjrc.com/teensy/index.html)
from [PJRC](http://www.pjrc.com).

PJRC's example keyboard and debug interface implementations were very useful
when helping to build my own implementations.  (I decided to build my own
implementations in C++ partially as a learning project, and also partially so I
could have some more re-usable code that could be shared across multiple
projects.)

The src/pjrc directory does contain some code taken directly from the PRJC
site, and includes PJRC license information.
