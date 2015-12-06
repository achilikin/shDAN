Test for modded MMR-70
======================

Depending on a project ([radio](https://github.com/achilikin/mmr70mod/blob/master/radio), [base](https://github.com/achilikin/mmr70mod/blob/master/base), [node](https://github.com/achilikin/mmr70mod/blob/master/node)) different I/O pins of MMR-70's ATmega32L can be populated. This firmware can be uploaded to ATmega32L for testing that everything is OK after soldering extra pins under a microscope :)

Also can be used to init/program Atmel I2C EEPROM 24Cxx series (up to 24C512) memory, read BMP180 sensor in I2C bus or DS18B20 on PB1 pin. 

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

**bmp180 commands:**
* _bmp init|poll_ - initialize/poll BMP180 sensor

**DS12B20 commands:**
* _ds init_ - initialize DS18B20 temperature sensor
* _ds start_ - start temperature conversion
* _ds read_ - read temperature from the scratchpad
* _ds get_ - start temperature conversion, wait and get t
* _ds data_ - read 16bit data from the scratchpad (bytes [2-3])
* _ds write_ - write uint16_t(utime) to the scratchpad

**Atmel I2C EEPROM 24C256 commands:**
* _i2cmem init [hex]_ - init memory by filling it with specified value, 0 by default
* _i2cmem write addr val_ - write byte at specified address
* _i2cmem dump [addr [len]]_ - dump memory, default address 0, length 256

**Debugging:**
* _mem_ - show available memory
* _led on|off_ - turn onboard LED on or off

For more information see [Readme](https://github.com/achilikin/mmr70mod/) in the project root directory.

**Current code size**
Version 2015-05-09
```
> Creating Symbol Table: test_main.sym
> avr-nm -n test_main.elf > test_main.sym
> Program:   11244 bytes (34.3% Full)
> Data:        521 bytes (25.4% Full)
> EEPROM:        2 bytes (0.2% Full)
```
