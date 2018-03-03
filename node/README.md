Data Node
=========

Data Node part of data acquisition network based on modified [Sony-Ericsson MMR-70 FM Music Transmitter](http://www.mikrocontroller.net/attachment/140251/MMR70.pdf) with new firmware written in pure C + avr-gcc (not Arduino). 

Depending on sensors attached to I2C/SPI buses and I/O pins exposed can be used to monitor temperature, humidity, pressure, digital/analogue signals, etc. Low power OLED SSD1306 display can be attached to show sensor readings and states. To talk to a remote base station RFM12BS (Si4420 based) transceiver is used.

I/O configuration
-----------------

| ATmega32L       | RFM12BS |
| --------------- |--------:|
| PB7 (SPI clock) | SCK     |
| PB6 (MISO)      | SDO     |
| PB5 (MOSI)      | SDI     |
| PB4 (SPI SS)    | SS      |
| PD3             | nIRQ    |
| ADC3            | ARSSI   |

| ATmega32L | Mode        |
| ----------|-------------|
| PB0       | Interactive |

Command line
------------
38400 baud serial connection (when node is not in power saving mode) can be used to control Data Node configuration and execute supported commands.

**Following general commands are available:**
* _help_ - show all supported commands
* _time_ - show current real-time clock time
* _reset_  - reset ATmega32
* _status_ - get some basic status info
* _calibrate_ - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary.

**Configuration:**
* _set nid N_ - set Node ID to N, 1 to 15 range
* _set tsync N_ - set time sync interval to every N data sessions
* _set led on|off_ - enable/disable on-board LED to for data poll indication  
* _set txpwr pwr_ - set RFN12BS transmit power, 0 to 7 range (0 - max, 7 - min)
* _set time HH:MM:SS_ - set RTC time, 24H format
* _set osccal X_ - set OSCCAL value for ATmega32 serial port 

**Digital/analogue inputs:**
* _poll_ - read T/H sensor connected to MMR-70
* _get pin_ - read digital pin, for example `get d3`
* _adc chan_ - read ADC channel, for example `adc 7`

**Debugging:**
* _mem_ - show available memory
* _echo rx|lsd|off_ - enable/disable data output to serial port

For more information see Readme in the project root directory.

**Makefile parameters**
To select Node ID pass _NID_ parameter when running make:
```
$make NID=5
```
To select RFM12 TX power pass _TXPWR_ parameter when running make:
```
$make TXPWR=1
```
To select OSCCAL power pass _OSCCAL_ parameter when running make:
```
$make OSCCAL=176
```

**Current code size**
Version 2018-03-03
```
> Creating Symbol Table: node_main.sym
> avr-nm -n node_main.elf > node_main.sym
> Program:   16850 bytes (51.4% Full)
> Data:        476 bytes (23.2% Full)
> EEPROM:        6 bytes (0.6% Full)
```
