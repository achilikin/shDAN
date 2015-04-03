FM Radio with RDS support 
=========================

Simple weather station using modified firmware for [MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf).
Similar to [FMBerry](https://github.com/Manawyrm/FMBerry), but running on MMR-70's ATmega32 itself instead of Raspberry Pi. Pure C + avr-gcc is used (not Arduino). 

Reads temperature and humidity data from local and remote T/H sensor (RHT03 or SHT1x) and displays data on attached OLED SSD1306 display:

![mmr70 screen](http://achilikin.com/github/mmr-mod-03.png)

and broadcasts RDS signal with sensor's readings:

![RDS](http://3.bp.blogspot.com/-cB2P4Qp3eOI/U4kIqX7pSPI/AAAAAAAAASs/hKfAir5Qco4/s1600/screenshot.png)

**Following general commands are available:**
* _help_ - show all supported commands
* _status_ - get some basic status info
* _reset_  - reset ATmega32
* _calibrate_ - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary.

**Time/date realted:**
* _time_ - show current time 
* _set time HH:MM:SS_ - set time, 24H format

**Digital/analogue inputs:**
* _poll_ - read T/H sensor connected to MMR-70
* _get pin_ - read digital pin, for example `get d3`
* _adc chan_ - read ADC channel, for example `adc 7` (only if analog pins are populated)

**Debugging:**
* _mem_ - show available memory
* _echo rht|adc|rds|off_ - enable/disable data output to serial port
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

**Current code size**
Version 2015-04-03
```
> Creating Symbol Table: radio_main.sym
> avr-nm -n radio_main.elf > radio_main.sym
> Program:   19910 bytes (60.8% Full)
> Data:        807 bytes (39.4% Full)
> EEPROM:       18 bytes (1.8% Full)
```
