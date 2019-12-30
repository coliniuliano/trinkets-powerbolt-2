# Weiser Powerbolt 2 ESP32 Interface #

* Also sold as Kwikset 907 Powerbolt 2.0
* May work on other keypad-based deadbolts

## Introduction ##

This is an example firmware that demonstrates an ability to interact with the Powerbolt 2 deadlock.  The intention is to enable remote Wifi management and access through an ESP32.

## Hardware ##

* SONIX SN8P2613 on both keypad and main body (datasheets available)
* 4-pin connection between keypad and main body consists of 3.3v, in, out, gnd
* One-wire protocol on each *in* and *out* pins

### Protocol ###
* Each packet is 10 bits long
* Each packet is sent twice with a 10ms delay between them (built in to the stop bit)
* Start bit = 30ms high and around 1.4ms low
* 8 data bits
  * Low = 0.3ms high and 0.7ms low
  * High = 0.7ms high and 0.3ms low
* Stop bit = 0.3ms high and 10ms low

## Research ##

* It is possible to send a key that does not have a corresponding button on the actual keypad.  In theory, a code containing this key would not be possible to enter using the physical keypad.  Such a code could only ever be entered through the ESP32 interface, meaning that it would be a remote-only key.
    * If the master code contained an unpressable key, the lock would not be able to be programmed physically without a reset, and would become remote-programmable only.
    * This is represented in the firmware as the *Unknown* key