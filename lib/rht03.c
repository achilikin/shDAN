/*  Reading RHT03 relative humidity/temperature sensor 
    Does not use floating point math to save space on ATtiny85
    Note: should work with DHT-22 as well

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
#include <avr/interrupt.h>
#include <util/delay.h>

#include "rht.h"
#include "rht03.h"
#include "pinio.h"
#include "timer.h"

#define RHTDATA     46 // start bit, 40 bits raw data, 5 decoded data

typedef struct rht03_s
{
	uint8_t  valid;   // data[] is valid
	uint8_t  data[5]; // 40 bits read from RHT03
	uint16_t errors;  // number of polling errors for debugging
	uint8_t  bits[RHTDATA]; // bit array read from RHT03
} rht03_t;

static rht03_t rht03;
static uint16_t EEMEM em_rht_errors = 0; // number of poll errors
static const uint8_t MAX_COUNTER = 0xFF;

void rht03_init(void)
{
	rht03.valid = 0;
	rht03.errors = eeprom_read_word(&em_rht_errors);
	memset(rht03.data, 0, 5);
	mmr_tp5_mode(INPUT_HIGHZ);
}

int8_t rht03_poll(rht_t *prht)
{
	uint8_t *bits = rht03.bits;
    uint8_t nbit = 0;
    uint8_t counter;
    uint8_t j = 0, i;

    // make sure that we have data pin HIGH
	mmr_tp5_mode(INPUT_HIGHZ);
    delay(50);
    // now pull it low for ~10 milliseconds
    mmr_tp5_dir(OUTPUT);
    delay(10);

    // read sensor's response
    mmr_tp5_dir(INPUT);

	// even without cli() reading is stable enough - 
	// we have only one timer interrupt anyway
	// cli();
    // wait for the sensor to pull down
    for(counter = 0; counter != MAX_COUNTER && mmr_tp5_get() != LOW; counter++);
    // measure sensor start bit HIGH state interval
    for(counter = 0; counter != MAX_COUNTER && mmr_tp5_get() == LOW; counter++);
    // sensor sets HIGH for about 80 usec, which close to ~76usec for "1" bit
    for(counter = 0; counter != MAX_COUNTER && mmr_tp5_get() != LOW; counter++);
    bits[nbit++] = counter; // for Arduino ATtiny85 @16MHZ ~18, @8Mhz ~8
    // read in 40 bits timing high state
    for(i = 0; i < 40; i++) {
        for(counter = 0; counter != MAX_COUNTER && mmr_tp5_get() == LOW; counter++);
        for(counter = 0; counter != MAX_COUNTER && mmr_tp5_get() != LOW; counter++);
        bits[nbit++] = counter;
    }
    //sei();

    uint8_t *pdata = &bits[41];
    pdata[0] = pdata[1] = pdata[2] = pdata[3] = pdata[4] = 0;

    // parse timings to bits, '1' stored as value close to bits[0]
    nbit = bits[0] / 2 + 1;
    for(i = 0; i < 40; i++) {
        if (bits[i+1] >= nbit) {
			j = (i & 0xF8) >> 3;
            pdata[j] |= (0x80 >> (i & 0x07));
		}
    }

    // if the checksum matches mark data valid and return true
    if ((uint8_t)(pdata[0] + pdata[1] + pdata[2] + pdata[3]) == pdata[4]) {
		memcpy(rht03.data, pdata, 5);
		rht03.valid = RHT_TVALID | RHT_HVALID;
        return 0;
    }
	
	rht03.errors++;
	eeprom_update_word(&em_rht_errors, rht03.errors);
    prht->errors = rht03.errors;
	return -1;
}

// parse data bits and return temperature in centigrades
int8_t rht03_get_temperature(rht_t *prht)
{
    int16_t temperature; 
	
	if (!rht03.valid)
		return -1;
    
	temperature = (int16_t)(rht03.data[2] & 0x7F);
    temperature <<= 8;
    temperature |= rht03.data[3];
	prht->temperature.dec = temperature % 10;
    temperature /= 10;
    if (rht03.data[2] & 0x80)
        temperature *= -1;
	prht->temperature.val = temperature;
	prht->valid |= RHT_TVALID;

	return 0;
}

// parse data bits and return relative humidity in %
int8_t rht03_get_humidity(rht_t *prht)
{ 
    uint16_t humidity;

	if (!rht03.valid)
		return -1;

    humidity = rht03.data[0];
    humidity <<= 8;
    humidity |= rht03.data[1];
	prht->humidity.dec = humidity % 10;
    humidity /= 10;
	prht->humidity.val = humidity;
	prht->valid |= RHT_HVALID;

    return 0;
}

void rht03_print(const char *data)
{
	const uint8_t *buf = rht03.bits;
	printf("%u %lu rht %s: ", rht03.errors, millis(), data ? "error" : "ok");
	for(uint8_t i = 0; i < 41; i++)
		printf("%u ", buf[i]);
	printf("| ");
	for(uint8_t i = 0; i < 5; i++)
		printf("%02x ", buf[i+41]);
	printf("| ");
	if (data != NULL)
		printf("%s", data);
	printf("\n");
}
