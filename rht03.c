/*	Polling of RHT03 relative humidity/temperature sensor 
	Does not use floating point math to save space on ATtiny85
	Note: should work with DHT-22 as well

	This copy is optimized for AVR Atmega32 on MMR-70
	Copyright (c) 2014 Andrey Chilikin (https://github.com/achilikin)
    
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
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "rht03.h"
#include "mmr70pin.h"

uint16_t EEMEM em_rht_errors = 0; // number of poll errors

static const uint8_t MAX_COUNTER = 0xFF;

void rht_init(rht03_t *prht)
{
	prht->valid = 0;
	prht->errors = eeprom_read_word(&em_rht_errors);
	memset(prht->data, 0, 5);
	mmr_tp5_mode(INPUT_HIGHZ);
}

int8_t rht_poll(rht03_t *prht)
{
	uint8_t *bits = prht->bits;
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
		memcpy(prht->data, pdata, 5);
		prht->valid = 1;
        return 0;
    }
	
	prht->errors ++;
	eeprom_update_word(&em_rht_errors, prht->errors);
    
	return -1;
}

// parse data bits and return temperature in centigrades
// if decimal is not NULL store decimal there
int8_t rht_getTemperature(rht03_t *prht, uint8_t *decimal)
{
    int16_t temperature; 
	
	if (!prht->valid)
		return RHT_ERROR;
    
	temperature = (int16_t)(prht->data[2] & 0x7F);
    temperature <<= 8;
    temperature |= prht->data[3];
    if (decimal)
        *decimal = temperature % 10;
    temperature /= 10;
    if (prht->data[2] & 0x80)
        temperature *= -1;

	return temperature;
}

// parse data bits and return relative humidity in %
// if decimal is not NULL store decimal there
int8_t rht_getHumidity(rht03_t *prht, uint8_t *decimal)
{ 
    uint16_t humidity;

	if (!prht->valid)
		return RHT_ERROR;

    humidity = prht->data[0];
    humidity <<= 8;
    humidity |= prht->data[1];
    if (decimal)
        *decimal = humidity % 10;
    humidity /= 10;

    return humidity;
}

