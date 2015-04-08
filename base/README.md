Base Station
============

Base Station part of data acquisition network based on modified [Sony-Ericsson MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf) with new firmware written in pure C + avr-gcc (not Arduino).

Reads temperature and humidity data from local and remote T/H sensor (RHT03 or SHT1x), pressure from local BMP180 digital pressure sensor and displays data on attached OLED SSD1306 display:

![mmr70 screen](http://achilikin.com/github/mmr-mod-03.png)

and broadcasts RDS signal with sensor's readings:

![RDS](http://3.bp.blogspot.com/-cB2P4Qp3eOI/U4kIqX7pSPI/AAAAAAAAASs/hKfAir5Qco4/s1600/screenshot.png)

To talk to remote data nodes RFM12BS (Si4420 based) transceiver is used.
 
[Blog](http://achilikin.blogspot.ie/2014/06/sony-ericsson-mmr-70-modding-extreme.html) with some extra pictures.

[NXP PCF2127](http://www.nxp.com/documents/data_sheet/PCF2127.pdf) real-time clock ([RasClock](http://afterthoughtsoftware.com/products/rasclock)) is used to synchronise time with data nodes and timestamp *log* output

**Following general commands are available:**
* _help_ - show all supported commands
* _reset_ - reset ATmega32
* _status_ - get some basic status info
* _calibrate_ - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary.

**Time/date realted:**
* _time_ - show current RTC time 
* _date_ - show current RTC date
* _set time HH:MM:SS_ - set RTC time, 24H format
* _set date YY/MM/DD_ - set RTC date

**Digital/analogue inputs:**
* _poll_ - read T/H sensor connected to MMR-70
* _get pin_ - read digital pin, for example `get d3`
* _adc chan_ - read ADC channel, for example `adc 7`

**Debugging:**
* _mem_ - show available memory
* _echo rx|rht|rds|dan|off_ - enable/disable data output to serial port
* _log on|off_ - enable/disable timestamped output of T/H readings


**Radio specific commands:**
* _radio on|off_
* _rdsid id_ - set RDS ID, stored in EEPROM
* _freq nnnn_ - set FM frequency, for example `freq 9700` for 97.00 MHz
* _txpwr 0-3_ - set NS741 transmitting power, store in EEPROM
* _rdstext_ - print RDS text being transmitted

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

For RFM12BS:
```
#define RFM_MODE RFM_MODE_RX // receive data from remote sensor (RX mode)
or
#define RFM_MODE RFM_MODE_TX // transmit data (TX mode)
```

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

**Current code size**
Version 2015-04-08
```
> Creating Symbol Table: base_main.sym
> avr-nm -n base_main.elf > base_main.sym
> Program:   26068 bytes (79.6% Full)
> Data:        900 bytes (43.9% Full)
> EEPROM:       19 bytes (1.9% Full)
```
