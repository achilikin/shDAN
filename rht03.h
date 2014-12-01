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

#ifndef _RHT03_MMR_70_H_
#define _RHT03_MMR_70_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick Visual Assist
}
#endif
#endif

#define RHTDATA     46 // start bit, 40 bits raw data, 5 decoded data
#define RHT_ERROR -120 // valid readings for humidity 0:100, temperature -40:+80

typedef struct rht03
{
	uint8_t  valid;   // data[] is valid
	uint8_t  data[5]; // 40 bits read from RHT03
	uint16_t errors;  // number of polling errors for debugging
	uint8_t  bits[RHTDATA]; // bit array read from RHT03
}rht03_t;

void rht_init(rht03_t *prht);
int8_t rht_poll(rht03_t *prht);
int8_t rht_getTemperature(rht03_t *prht, uint8_t *decimal);
int8_t rht_getHumidity(rht03_t *prht, uint8_t *decimal);

#ifdef __cplusplus
}
#endif
#endif


