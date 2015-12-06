/* Simple DS18x20 API for AVR
  
   Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
   https://opensource.org/licenses/MIT
*/
#ifndef __DS18X20_PIN_H__
#define __DS18X20_PIN_H__

#include <stdint.h>
#include <avr/io.h>

#define DS18x_PAD_LEN 9
#define DS18x_CTIME 150 // max conversion time, ms

// DS18x20 commands
#define DS18x_CMD_COVERT    0x44
#define DS18x_CMD_SKIP_ROM  0xCC
#define DS18x_CMD_READ_PAD  0xBE
#define DS18x_CMD_WRITE_PAD 0x4E
#define DS18x_CMD_COPY_PAD  0x48

// DS18x20 errors
#define DS18x_EOK     0
#define DS18x_EPIN   -1 // not present, wrong pin?
#define DS18x_ESHORT -2 // short circuit detected
#define DS18x_ETOUT  -3 // conversion timeout
#define DS18x_ECRC   -4 // wrong crc

typedef struct ds_temp_s {
	int8_t  val;
	uint8_t dec;
} ds_temp_t;

// checks is sensor is present, sets 9 bit precision
// and starts temperature conversion
#define DSx18_TYPE_B 0
#define DSx18_TYPE_S 1

int8_t ds18x_reset(uint8_t pin);
int8_t ds18x_init(uint8_t pin, uint8_t type);

// sends specified command to DS18x20
int8_t ds18x_cmd(uint8_t pin, uint8_t cmd);

// starts new conversion and then reads temperature
int8_t ds18x_get_temp(uint8_t pin, ds_temp_t *t, uint8_t *pad);

// reads scratchpad and last converted temperature
int8_t ds18x_read_temp(uint8_t pin, ds_temp_t *t, uint8_t *pad);

// use Th and Tl as general storage in the scratchpad
int8_t ds18x_wite_data(uint8_t pin, uint16_t data);
int8_t ds18x_read_data(uint8_t pin, uint16_t *data);

#endif
