/*  Reading SHT1x relative humidity/temperature sensors
    http://www.sensirion.com/nc/en/products/humidity-temperature/download-center/

    This copy is optimized for AVR Atmega32 on MMR-70
    Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "rht.h"
#include "sht1x.h"
#include "pinio.h"
#include "timer.h"

// ack timeout
#define SHT1X_TIMEOUT 128

// crc calculation seeds for different commands
#define SHT1X_CRC_TSEED 0x53
#define SHT1X_CRC_HSEED 0xF5

#define SHT1X_CMD_TEMPERATURE 0x03
#define SHT1X_CMD_HUMIDITY    0x05
#define SHT1X_CMD_GET_STATUS  0x07

#define sht_data_mode(x) mmr_tp5_mode(x)
#define sht_data(x) mmr_tp5_set(x)
#define sht_ack(x) mmr_tp5_get(x)

#define sht_sck_mode(x) mmr_tp4_mode(x)
#define sht_sck(x) mmr_tp4_set(x)

typedef struct sht1x_s
{
	uint8_t  valid;   // upper 6 bit - last command sent
	uint8_t  status;  // status byte from SHT1x, should be 0
	uint16_t errors;  // number of errors
	uint8_t  data[3]; // 16 data bits, crc
	uint8_t  dtype;   // data type - 'T' or 'H'
	uint16_t draw;    // data raw value
	u8val_t  t, h;    // data in decimal format
	double   ft, fh;  // data in double format
} sht1x_t;

static sht1x_t sht;
static uint16_t EEMEM em_sht_errors = 0; // number of poll errors

// reset SHT1x communication
static void sht1x_reset(void)
{
	sht_sck_mode(OUTPUT_LOW);
	sht_data_mode(INPUT_HIGHZ);

	for(int8_t i = 1; i < 21; i++) {
		sht_sck(i & 0x01);
	}
}

// generate one tick on clock line
static inline void sht1x_tick(void)
{
	for(int8_t i = 1; i < 3; i++) {
		sht_sck(i & 0x01);
	}
}

// generate communication start condition
static inline void sht1x_start(void)
{
	sht_sck(HIGH);
	sht_data_mode(OUTPUT_LOW);
	sht_sck(LOW);
	sht_sck(HIGH);
	sht_data_mode(INPUT_HIGHZ);
	sht_sck(LOW);
}

// send 8 bit command to SHT1x
static int8_t sht1x_send(uint8_t cmd)
{
	uint8_t i;
	cmd &= 0x1F; // upper 3 bits should be 0

	sht1x_start();
	sht_data_mode(OUTPUT_LOW);

	// shift out command bits
	for(i = 0x80; i; i >>= 1) {
		if (cmd & i)
			sht_data_mode(INPUT_HIGHZ);
		else
			sht_data_mode(OUTPUT_LOW);
		sht1x_tick();
	}
	// wait for ACK
	sht_data_mode(INPUT_HIGHZ);
	for(i = 0; i < SHT1X_TIMEOUT; i++) {
		if (sht_ack() == 0)
			break;
	}
	if (i == SHT1X_TIMEOUT) {
//		printf_P(PSTR("SHT1x: No ACK on send\n"));
		sht.errors++;
		return -1;
	}
	// reset ACK
	sht1x_tick();

	// store last command
	sht.valid &= RHT_TVALID | RHT_HVALID;
	sht.valid |= (cmd << 2);
	return 0;
}

// read data in normal (msb = 1) or reverse order (msb = 0)
static int8_t sht1x_read(uint8_t *buf, uint8_t msb)
{
	uint8_t i, rbyte = 0;

	for(i = 0; i < 8; i++) {
		sht_sck(HIGH);
		if (sht_ack()) {
			if (msb)
				rbyte |= 0x80 >> i;
			else
				rbyte |= 0x01 << i;
		}
		sht_sck(LOW);
	}

	// ack read
	sht_data_mode(OUTPUT_LOW);
	sht1x_tick();
	sht_data_mode(INPUT_HIGHZ);

	*buf = rbyte;
	return 0;
}

// get status byte from SHT1x
// can be used to check if SHT1x is present
static int8_t sht1x_read_status(sht1x_t *psh)
{
	union {
		uint16_t check;
		uint8_t  byte[2];
	} status;

	sht1x_send(SHT1X_CMD_GET_STATUS);
	sht1x_read(&status.byte[0], 0);
	sht1x_read(&status.byte[1], 1);
	psh->status = status.byte[0];

	return (status.check == 0x7500) ? 0 : -1;
}

static uint8_t sht1x_crc(uint8_t data, uint8_t seed)
{
	uint8_t crc = seed;

	for(uint8_t i = 0; i < 8; i++) {
		uint8_t bit = crc & 0x80;
		crc <<= 1;
		if (((data << i) & 0x80) ^ bit) {
			crc |= 0x01;
			crc ^= 0x30;
		}
	}

	return crc;
}

static int8_t sht1x_read_data(sht1x_t *psh)
{
	sht1x_read(&psh->data[0], 1);
	sht1x_read(&psh->data[1], 1);
	sht1x_read(&psh->data[2], 0); // we read crc byte LSB first

	// store raw data as 16 bit value
	psh->draw = psh->data[0];
	psh->draw <<= 8;
	psh->draw |= psh->data[1];

	return 0;
}

static int8_t sht1x_parse_temperature(sht1x_t *psh)
{
	uint8_t crc;
	crc = sht1x_crc(psh->data[0], SHT1X_CRC_TSEED);
	crc = sht1x_crc(psh->data[1], crc);
	if (crc != psh->data[2]) {
		psh->errors++;
		return -1;
	}

	double ft = -39.66 + 0.01*psh->draw; // for 3.3V
	sht.ft = ft;
	int16_t tv = (int16_t)(ft*100.0);
	sht.t.val = tv/100;
	sht.t.dec = (tv%100)/10;
	sht.valid |= RHT_TVALID;
	sht.dtype = 'T';
	return 0;
}

static int8_t sht1x_parse_humidity(sht1x_t *psh)
{
	uint8_t crc;
	crc = sht1x_crc(psh->data[0], SHT1X_CRC_HSEED);
	crc = sht1x_crc(psh->data[1], crc);
	if (crc != psh->data[2]) {
		psh->errors++;
		return -1;
	}

	uint16_t h = psh->draw;
	double fh = -2.0468 + 0.0367*h + -1.5955e-6 * (h*h);
	double ch = (sht.ft - 25.0)*(0.01 + 0.00008*h) + fh;
	sht.fh = ch;
	uint16_t hv = (int16_t)(ch*100.0);
	sht.h.val = hv/100;
	sht.h.dec = (hv%100)/10;
	sht.valid |= RHT_HVALID;
	sht.dtype = 'H';
	
	return 0;
}

// public functions used by rht_*() API ---------------------------------------

void sht1x_init(void)
{
	sht.valid = 0;
	sht.t.val = 0;
	sht.t.dec = 0;
	sht.h.val = 0;
	sht.h.dec = 0;
	sht.errors = eeprom_read_word(&em_sht_errors);

	sht_data_mode(INPUT_HIGHZ);
	sht_sck_mode(OUTPUT_LOW);

	sht1x_reset();
	delay(2);
	sht1x_read_status(&sht);
	delay(2);
	sht1x_send(SHT1X_CMD_TEMPERATURE);
}

void sht1x_print(const char *data)
{
	u8val_t *pval = (sht.dtype == 'T') ? &sht.t : &sht.h;

	printf_P(PSTR("%u %lu "), sht.errors, millis());
	if (!data)
		data = "error";
	printf_P(PSTR("%c: %02X %02X %02X | %04d %d.%02d | %s\n"), sht.dtype,
		sht.data[0], sht.data[1], sht.data[2],
		sht.draw, pval->val, pval->dec, data);
}

int8_t sht1x_poll(rht_t *psht)
{
	// if data line is low - SHT1x is ready
	// read data and parse it
	if (sht_ack() == 0) {
		sht1x_read_data(&sht);
		if ((sht.valid >> 2) == SHT1X_CMD_TEMPERATURE)
			sht1x_parse_temperature(&sht);
		if ((sht.valid >> 2) == SHT1X_CMD_HUMIDITY)
			sht1x_parse_humidity(&sht);
	}

	// request new data from SHT1x
	if ((sht.valid >> 2) == SHT1X_CMD_TEMPERATURE)
		sht1x_send(SHT1X_CMD_HUMIDITY);
	else
		sht1x_send(SHT1X_CMD_TEMPERATURE);

	if (psht->errors != sht.errors) {
		psht->errors = sht.errors;
		eeprom_update_word(&em_sht_errors, sht.errors);
		return -1;
	}

	return 0;
}

int8_t sht1x_get_temperature(rht_t *prht)
{
	if (sht.valid & RHT_TVALID) {
		prht->valid |= RHT_TVALID;
		prht->temperature.val = sht.t.val;
		prht->temperature.dec = sht.t.dec;
		return 0;
	}
	return -1;
}

int8_t sht1x_get_humidity(rht_t *prht)
{
	if (sht.valid & RHT_HVALID) {
		prht->valid |= RHT_HVALID;
		prht->humidity.val = sht.h.val;
		prht->humidity.dec = sht.h.dec;
		return 0;
	}
	return -1;
}
