mmr70mod
========

Weather station using modified firmware for [MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf).
Similar to [FMBerry](https://github.com/Manawyrm/FMBerry), but running on MMR-70's ATmega32 itself instead of Raspberry Pi.

Reads temperature and humidity data from T/H sensor (RHT03 or SHT1x), pressure from BMP180 digital pressure and displays data on attached OLED SSD1306 display:

![mmr70 screen](http://achilikin.com/github/mmr-mod-03.png)

and broadcasts RDS signal with sensor's readings:

![RDS](http://3.bp.blogspot.com/-cB2P4Qp3eOI/U4kIqX7pSPI/AAAAAAAAASs/hKfAir5Qco4/s1600/screenshot.png)

[Blog](http://achilikin.blogspot.ie/2014/06/sony-ericsson-mmr-70-modding-extreme.html) with some extra pictures.

If RTC ([NXP PCF2127](http://www.nxp.com/documents/data_sheet/PCF2127.pdf), for example, [RasClock](http://afterthoughtsoftware.com/products/rasclock)) is detected then it will be used to timestamp data for *log* output

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
* _help_ - show all supported commands
* _status_ - get some basic status info
* _reset_  - reset ATmega32
* _calibrate_ - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary.

**Digital/analogue inputs:**
* _poll_ - read T/H sensor connected to MMR-70
* _adc chan_ - read ADC channel, for example `adc 7`

**Debugging:**
* _mem_ - show available memory
* _debug rht|adc|rds|off_ - enable/disable debug output to serial port
* _log on|off_ - enable/disable timestamped output of T/H readings


**Radio specific commands:**
* _rdsid id_ - set RDS ID, stored in EEPROM
* _rdstext text_ - manually set RDS text instead of RHT03 readings
* _freq nnnn_ - set FM frequency, for example `freq 9700` for 97.00 MHz
* _txpwr 0-3_ - set NS741 transmitting power, store in EEPROM
* _volume 0-6_ - set volume, stored in EEPROM
* _mute on|off_ - turns off RDS signal as well
* _stereo on|off_ - better to keep it on as some RDS receivers do not work without pilot signal
* _radio on|off_
* _gain low|off_ - set input signal gain low to -9dB

Code Customization
------------------

First get familiar with the code.

Select T/H sensor by editing rht.h. For RHT03:
```
#define RHT_TYPE RHT_TYPE_RHT03
```
and connect RHT03 Data to tp5.

For SHT1x:
```
#define RHT_TYPE RHT_TYPE_SHT10
```
and connect SCK to tp4 and Data to tp5. 

For new versions of UART/I2C libraries check [Peter Fleury's page](http://homepage.hispeed.ch/peterfleury/avr-software.html)

If you managed to solder some wires to Analogue Inputs then define populated ADCs using **ADC_MASK**.
For example, if ADC 4 to 7 populated, then define:
```
#define ADC_MASK 0xF0
```

If you experience unstable communication try to calibrate **OSCCAL** value and then change ```uint8_t EEMEM em_osccal``` in the *main.c*

Using different clock speed for ATmega32:

1. Change fuses line in Makefile. [AVR Fuses calculator](http://www.engbedded.com/fusecalc)
2. Change init_millis(), as now it is hardcoded to for F_CPU = 8MHz
3. Make sure to select [proper speed](http://www.wormfood.net/avrbaudcalc.php) for serial communication

Useful links not listed above
-----------------------------

* MMR-70 discussion at [www.mikrocontroller.net](http://www.mikrocontroller.net/topic/252124)
* MMR-70 hack: [ISP-Programmmer](http://www.elektronik-labor.de/AVR/MMR70_2.html)
* How to protect Raspberry Pi GPIOs with a [74LVC244](http://blog.stevemarple.co.uk/2012/07/avrarduino-isp-programmer-using.html) buffer
