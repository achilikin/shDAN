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
#include <stdio.h>
#include <util/crc16.h>
#include <util/delay.h>
#include <util/atomic.h>

#include "ds18x.h"
#include "pinio.h"

#define DS_Tstart 1 // start time
#define DS_Trec   2 // recovery time, the longer wire, the longer time

int8_t ds18x_reset(uint8_t pin)
{
	pinMode(pin, OUTPUT_LOW);
	_delay_us(480);
	pinMode(pin, INPUT_UP);
	_delay_us(65);
	// 0 - present, else - not present
	int8_t err = digitalRead(pin);
	// check for short circuit 
	_delay_us(240);
	if (digitalRead(pin) == 0)
		err = 2;
	return -err;
}

static inline int8_t ds18x_rbit(uint8_t pin)
{
	uint8_t bit = 1;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		pinMode(pin, OUTPUT_LOW);
		_delay_us(DS_Tstart);
		pinDir(pin, INPUT);
		_delay_us(10);
		for (uint8_t i = 0; i < 5; i++) {
			if (digitalRead(pin) == 0)
				bit = 0;
			_delay_us(1);
		}
		_delay_us(45);
	}
	_delay_us(DS_Trec);
	return bit;								
}

static uint8_t ds18x_read(uint8_t pin)
{
	uint8_t data = 0x00;
	for (uint8_t i = 0; i < 8; i++)
		data |= ds18x_rbit(pin) << i;

	return data;
}

static inline void ds18x_wbit(uint8_t pin, uint8_t bit)
{
	pinMode(pin, OUTPUT_LOW);
	if (bit) {
		_delay_us(5);
		pinDir(pin, INPUT);
		_delay_us(55);
	}
	else {
		_delay_us(55);
		pinDir(pin, INPUT);
		_delay_us(5);
	}
}

static void ds18x_write(uint8_t pin, uint8_t data)
{
	for(uint8_t i = 0; i < 8; i++) {
		ds18x_wbit(pin, data & 0x01);
		_delay_us(DS_Trec);
		data >>= 1;
	}
}

int8_t ds18x_cmd(uint8_t pin, uint8_t cmd)
{
	int8_t error = ds18x_reset(pin);
	if (!error) {
		ds18x_write(pin, DS18x_CMD_SKIP_ROM);
		ds18x_write(pin, cmd);
	}
	return error;
}

static int8_t ds18x_read_pad(uint8_t pin, uint8_t *pad)
{
	int8_t error = ds18x_cmd(pin, DS18x_CMD_READ_PAD);
	if (!error) {
		for(uint8_t i = 0; i < DS18x_PAD_LEN; i++) 
			pad[i] = ds18x_read(pin);
		uint8_t crc = 0;
		for (uint8_t i = 0; i < (DS18x_PAD_LEN - 1); i++)
			crc = _crc_ibutton_update(crc, pad[i]);
		if (crc != pad[DS18x_PAD_LEN - 1])
			return DS18x_ECRC;
	}
	return error;
}

int8_t ds18x_init(uint8_t pin, uint8_t type)
{
	if (type == DSx18_TYPE_B) {
		uint8_t pad[DS18x_PAD_LEN];
		if (ds18x_read_pad(pin, pad) == 0) {
			pad[4] = 0x3F; // 9 bit temperature conversion
			ds18x_cmd(pin, DS18x_CMD_WRITE_PAD);
			ds18x_write(pin, pad[2]);
			ds18x_write(pin, pad[3]);
			ds18x_write(pin, pad[4]);
		}
	}

	// start temperature conversion
	return ds18x_cmd(pin, DS18x_CMD_COVERT);
}

int8_t ds18x_wite_data(uint8_t pin, uint16_t data)
{
	uint8_t pad[DS18x_PAD_LEN];
	int8_t error = ds18x_read_pad(pin, pad);
	if (!error) {
		uint16_t *ptr = (uint16_t *)pad;
		ptr[1] = data;
		ds18x_cmd(pin, DS18x_CMD_WRITE_PAD);
		ds18x_write(pin, pad[2]);
		ds18x_write(pin, pad[3]);
		ds18x_write(pin, pad[4]);
		ds18x_cmd(pin, DS18x_CMD_COPY_PAD);
	}
	return error;

}

int8_t ds18x_read_data(uint8_t pin, uint16_t *data)
{
	uint8_t pad[DS18x_PAD_LEN];
	int8_t error = ds18x_read_pad(pin, pad);
	if (!error) {
		uint16_t *ptr = (uint16_t *)pad;
		*data = ptr[1];
	}
	return error;
}

int8_t ds18x_read_temp(uint8_t pin, ds_temp_t *t, uint8_t *buf)
{
	uint8_t local[DS18x_PAD_LEN];
	uint8_t *pad = local;
	if (buf)
		pad = buf;

	int8_t error = ds18x_read_pad(pin, pad);
	if (!error) {
		t->val = (pad[1] << 4) | (pad[0] >> 4);
		if (t->val & 0x80)
			t->val = -(t->val & 0x7F);
		t->dec = ((pad[0] >> 3) & 0x01) * 5;
	}

	return error;
}

int8_t ds18x_get_temp(uint8_t pin, ds_temp_t *t, uint8_t *buf)
{
	int8_t error = ds18x_cmd(pin, DS18x_CMD_COVERT);
	if (error == 0) {
		uint16_t i = 0;
		while (ds18x_rbit(pin) == 0 && i < DS18x_CTIME) {
			_delay_ms(1);
			i++;
		}
		if (i == DS18x_CTIME)
			return DS18x_ETOUT;
		error = ds18x_read_temp(pin, t, buf);
	}
	return error;
}
