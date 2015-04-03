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

#ifndef RHT03_MMR_70_H
#define RHT03_MMR_70_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick Visual Assist
}
#endif
#endif

void   rht03_init(void);
void   rht03_print(const char *data);
int8_t rht03_poll(rht_t *prht);
int8_t rht03_get_temperature(rht_t *prht);
int8_t rht03_get_humidity(rht_t *prht);

#ifdef __cplusplus
}
#endif
#endif


