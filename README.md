mmr70mod
========

Modified firmware for [MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf).
Similar to [FMBerry](https://github.com/Manawyrm/FMBerry), but running on ATmega32 of MMR-70 itself instead of Raspberry Pi
and broadcasting RDS signal with temperature and humidity readings from RHT03 sensor connected to TP5:
![RDS](http://3.bp.blogspot.com/-cB2P4Qp3eOI/U4kIqX7pSPI/AAAAAAAAASs/hKfAir5Qco4/s1600/screenshot.png)

[Blog](http://achilikin.blogspot.ie/2014/06/sony-ericsson-mmr-70-modding-extreme.html) with some extra pictures.

Avrdude on Raspberry Pi
-----------------------

I use RPi as AVR programmer, and this is how it can be done:

1. Install [avrdude](http://kevincuzner.com/2013/05/27/raspberry-pi-as-an-avr-programmer/) for Raspberry Pi
2. If you want to use different RPi pin for Reset then edit `/usr/local/etc/avrdude.conf`, section *programmer*
3. To save four keystrokes every time when you program AVR add setuid to `/usr/local/bin/avrdude`, 
so you will not need to add `sudo` every time when you want to run `make program`, `make fuses`, `make reset`

Building the firmware
---------------------

Clone code from github, connect MMR-70 to Raspberry Pi as ISP or wnatever you use for avr-gcc/avrdude.

Make it: `make`, burn ATmega fuses to 8MHz: `make fuses`, upload firmware: `make program`.

Connect to MMR-70 serial port, open console (I'd recommend [Tera Term](http://ttssh2.sourceforge.jp/index.html.en)).

**Following general commands are available:**
* status - get some basic status info
* reset  - reset ATmega32
* calibrate - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary

**Digital/analogue inputs:**
* poll - read RHT03 sensor connected to TP5
* adc chan - read ADC channel, for example `adc 7`

**Debugging:**
* debug rht|adc|rds|off - enable/disable debug output to serial port

**Radio specific commands:**
* rdsid id - set RDS ID, stored in EEPROM
* rdstext text - manually set RDS text instead of RHT03 readings
* freq nnnn - set FM frequency, for example `freq 9700` for 97.00 MHz
* txpwr 0-3 - set NS741 transmitting power, store in EEPROM
* volume 0-6 - set volume, stored in EEPROM
* mute on|off
* stereo on|off - better to keep it on as some RDS receivers  do not work without pilot signal
* radio on|off
* gain low|off - set input signal gain low to -9dB


Code Customization
------------------

First get familiar with the code. 

For new versions of UART/I2C libraries check
[Peter Fleury's page](http://homepage.hispeed.ch/peterfleury/avr-software.html)

If you managed to solder some wires to Analogue Inputs then define populated ADCs using **ADC_MASK**.
For example, if ADC 4 to 7 populated, then define:
```
#define ADC_MASK 0xF0
```

If you experience unstable communication try to calibrate **OSCCAL** value and then set LOAD_OSCCAL to 1
```
#define LOAD_OSCCAL 0
```

Using different clock speed for ATmega32:
1. Change fuses line in Makefile. [AVR Fuses calculator](http://www.engbedded.com/fusecalc)
2. Change init_millis(), as not it is hardcoded to for F_CPU = 8MHz
3. Make sure to select [proper speed](http://www.wormfood.net/avrbaudcalc.php) for serial communication

Useful links not listed above
-----------------------------

* Discussion at [www.mikrocontroller.net](http://www.mikrocontroller.net/topic/252124)
* MMR-70 Hack: [ISP-Programmmer](http://www.elektronik-labor.de/AVR/MMR70_2.html)
