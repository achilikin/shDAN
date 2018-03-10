/*  Polling relative humidity/temperature sensors RHT03/SHT1x

    This copy is optimized for AVR Atmega32 on MMR-70
    Copyright (c) 2018 Andrey Chilikin (https://github.com/achilikin)
    
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
#ifndef RHT_MMR_70_H
#define RHT_MMR_70_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick Visual Assist
}
#endif
#endif

#define RHT_TYPE_RHT03 1
#define RHT_TYPE_SHT10 2

#ifndef RHT_TYPE
#error RHT_TYPE must be defined in Makefile, use -DRHT_TYPE=RHT_TYPE_RHT03 or -DRHT_TYPE=RHT_TYPE_SHT10
#endif

#define RHT_TVALID 0x01
#define RHT_HVALID 0x02

typedef struct u8val_s
{
	uint8_t val; // high bit: negative
	uint8_t dec;
} u8val_t;

static inline int8_t get_u8val(uint8_t val)
{
	if (val & 0x80)
		return -(val & 0x7F);
	return val;
}

typedef struct rht_s
{
	uint8_t  valid;
	u8val_t  humidity;
	u8val_t  temperature;
	uint16_t errors; // number of polling errors for debugging
} rht_t;

#if (RHT_TYPE == RHT_TYPE_SHT10)
#include "sht1x.h"
static inline void   rht_init(void) { sht1x_init(); }
static inline void   rht_print(const char *data) { sht1x_print(data); }
static inline int8_t rht_poll(rht_t *prht) { return sht1x_poll(prht); }
static inline int8_t rht_get_temperature(rht_t *prht) { return sht1x_get_temperature(prht); }
static inline int8_t rht_get_humidity(rht_t *prht)  { return sht1x_get_humidity(prht); }
#else
#include "rht03.h"
static inline void   rht_init(void) { rht03_init(); }
static inline void   rht_print(const char *data) { rht03_print(data); }
static inline int8_t rht_poll(rht_t *prht) { return rht03_poll(prht); }
static inline int8_t rht_get_temperature(rht_t *prht) { return rht03_get_temperature(prht); }
static inline int8_t rht_get_humidity(rht_t *prht)  { return rht03_get_humidity(prht); }
#endif

// dst must be at least 61 byte long
int8_t rht_read(rht_t *rht, int8_t echo, char *dst);

#ifdef __cplusplus
}
#endif
#endif


