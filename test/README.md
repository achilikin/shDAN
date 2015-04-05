Test for modded MMR-70
======================

Depending on a project ([radio](https://github.com/achilikin/mmr70mod/blob/master/radio), [base](https://github.com/achilikin/mmr70mod/blob/master/base), [node](https://github.com/achilikin/mmr70mod/blob/master/node)) different I/O pins of MMR-70's ATmega32L can be populated. This firmware can be uploaded to ATmega32L for testing that everything is OK after soldering under a microscope :)

Compile and upload firmware as usual, and the use serial console for testing.

Supported commands
-----------------

**Following general commands are available:**
* _help_ - show all supported commands + firmware version
* _time_ - show current software clock time
* _reset_  - reset ATmega32
* _status_ - get some basic status info

**Configuration:**
* _set time HH:mm:ss_ - set software time, 24H format
* _set osccal X_ - set OSCCAL value for ATmega32 serial port 
* _calibrate_ - in case if serial communication is not stable, try to run calibration and check if OSCCAL value is too close to upper or lower boundary.

**Digital/analogue inputs:**
* _adc chan_ - read ADC channel, for example `adc 7`
* _get pin_ - read digital pin, for example `get d3`
* _set pin val_ - write to digital pin, for example `set d3 0` 

**Debugging:**
* _mem_ - show available memory
* _led on|off_ - turn onboard LED on or off

For more information see [Readme](https://github.com/achilikin/mmr70mod/) in the project root directory.

**Current code size**
Version 2015-04-05
```
> Creating Symbol Table: test_main.sym
> avr-nm -n test_main.elf > test_main.sym
> Program:    7714 bytes (23.5% Full)
> Data:        394 bytes (19.2% Full)
> EEPROM:        2 bytes (0.2% Full)
```
