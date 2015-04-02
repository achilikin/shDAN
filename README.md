MMR-70 modding
==============

Modified firmware for [MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf).

Includes different projects:

**FM Radio with RDS support firmware**
[RADIO](https://github.com/achilikin/mmr70mod/blob/master/radio) - Similar to [FMBerry](https://github.com/Manawyrm/FMBerry), but running on MMR-70's ATmega32 itself instead of Raspberry Pi. Pure C + avr-gcc is used (not Arduino).

**Data Acquisition Network**
[BASE](https://github.com/achilikin/mmr70mod/blob/master/base) - Base Station for data acquisition network
[NODE](https://github.com/achilikin/mmr70mod/blob/master/node) - Data Node for data acquisition network

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

In the project's folder: `make` to make it, burn ATmega fuses to 8MHz: `make fuses`, upload firmware: `make program`.

Serial console
--------------
For new versions of UART/I2C libraries check [Peter Fleury's page](http://homepage.hispeed.ch/peterfleury/avr-software.html)

Connect to MMR-70 serial port, open console (I'd recommend [Tera Term](http://ttssh2.sourceforge.jp/index.html.en)).

Useful links
------------

* MMR-70 discussion at [www.mikrocontroller.net](http://www.mikrocontroller.net/topic/252124)
* MMR-70 as [ISP-Programmmer](http://www.elektronik-labor.de/AVR/MMR70_2.html)
* How to protect Raspberry Pi GPIOs with a [74LVC244](http://blog.stevemarple.co.uk/2012/07/avrarduino-isp-programmer-using.html) buffer
* RFM12BS ARSSI [hack](http://blog.strobotics.com.au/2008/06/17/rfm12-tutorial-part2)


