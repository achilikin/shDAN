shDAN: small house Data Acquisition Network
==========================================
[BASE](https://github.com/achilikin/shDAN/blob/master/base) - Base Station firmware for [Data Acquisition Network](https://github.com/achilikin/shDAN/blob/master/hDAN.md)

[NODE](https://github.com/achilikin/shDAN/blob/master/node) - Data Node for [Data Acquisition Network](https://github.com/achilikin/shDAN/blob/master/hDAN.md)

Topology
--------
![hDAN topology](https://rawgithub.com/achilikin/shDAN/master/hDAN_topology.svg)

Protocol
--------

![hDAN protocol](https://rawgithub.com/achilikin/shDAN/master/hDAN_protocol.svg)

Messages
--------

![hDAN messages](https://rawgithub.com/achilikin/shDAN/master/hDAN_messages.svg)

**At the moment all projects under major re-factoring/code clean-up**


RFM12BS radio module
--------------------

[RFM12B wireless FSK transceiver module](http://www.hoperf.com/rf/fsk_module/RFM12B.htm)
[Si4420/21 RF transmitters](http://www.silabs.com/products/wireless/EZRadio/Pages/Si442021.aspx)
[RFM12BS ARSSI hack](http://blog.strobotics.com.au/2008/06/17/rfm12-tutorial-part2)

Atmega 32L - MMR-70 modding
---------------------------

Base station and nodes are built using modified firmware for Atmega 32L based [MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf).

Includes different projects:

**Modding test**

[TEST](https://github.com/achilikin/shDAN/blob/master/test) - for testing freshly modified MMR-70s

**FM Radio with RDS support firmware**

[RADIO](https://github.com/achilikin/shDAN/blob/master/radio) - Similar to [FMBerry](https://github.com/Manawyrm/FMBerry), but running on MMR-70's ATmega32 itself instead of Raspberry Pi.

Avrdude on Raspberry Pi
-----------------------
I use RPi as AVR programmer, and this is how it can be done:

1. Install [avrdude](http://kevincuzner.com/2013/05/27/raspberry-pi-as-an-avr-programmer/) for Raspberry Pi
2. If you want to use different RPi pin for Reset then edit `/usr/local/etc/avrdude.conf`, section *programmer*
3. To save four keystrokes every time when you program AVR add setuid to `/usr/local/bin/avrdude`, 
so you will not need to add `sudo` every time when you want to run `make program`, `make fuses`, `make reset`

Building the firmware
---------------------
Clone code from github, connect MMR-70 to Raspberry Pi as ISP or whatever you use for avr-gcc/avrdude.

In the project's folder: `make` to make it, burn ATmega fuses to 8MHz: `make fuses` (once off operation), upload firmware: `make program`.

Useful links
------------

* MMR-70 discussion at [www.mikrocontroller.net](http://www.mikrocontroller.net/topic/252124)
* MMR-70 as [ISP-Programmmer](http://www.elektronik-labor.de/AVR/MMR70_2.html)
* How to protect Raspberry Pi GPIOs with a [74LVC244](http://blog.stevemarple.co.uk/2012/07/avrarduino-isp-programmer-using.html) buffer
* [Tera Term](http://ttssh2.sourceforge.jp/index.html.en) useful terminal application
* [UART/I2C](http://homepage.hispeed.ch/peterfleury/avr-software.html) Peter Fleury's UART/I2C libraries 