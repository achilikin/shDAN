/*  Polling relative humidity/temperature sensors RHT03/SHT1x

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
#include <avr/pgmspace.h>

#include "rht.h"
#include "timer.h"

// poll t/rh sensor and update RDS radio text
// dst must be at least 61 byte long
int8_t rht_read(rht_t *rht, int8_t echo, char *dst)
{
	int8_t ret = rht_poll(rht);
	if (ret == 0) {
		rht_get_temperature(rht);
		rht_get_humidity(rht);
		int8_t val = get_u8val(rht->temperature.val);
		sprintf_P(dst, PSTR("T %d.%02d H %d.%02d"),
			val, rht->temperature.dec,
			rht->humidity.val, rht->humidity.dec);
	}
	if (echo)
		rht_print(ret == 0 ? dst : NULL);

	return ret;
}
