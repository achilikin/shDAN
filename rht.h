/*  Polling relative humidity/temperature sensors RHT03/SHT1x

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
#ifndef __RHT_MMR_70_H__
#define __RHT_MMR_70_H__

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

// select which sensor is active
#define RHT_TYPE RHT_TYPE_SHT10

#define RHT_TVALID 0x01
#define RHT_HVALID 0x02

typedef struct u8val_s
{
	int8_t  val;
	uint8_t dec;
} u8val_t;

typedef struct rht_s
{
	uint8_t  valid;
	u8val_t  humidity;
	u8val_t  temperature;
	uint16_t errors; // number of polling errors for debugging
} rht_t;

#if (RHT_TYPE == RHT_TYPE_RHT03)
#include "rht03.h"
static inline void   rht_init(void) { rht03_init(); }
static inline void   rht_print(const char *data) { rht03_print(data); }
static inline int8_t rht_poll(rht_t *prht) { return rht03_poll(prht); }
static inline int8_t rht_getTemperature(rht_t *prht) { return rht03_getTemperature(prht); }
static inline int8_t rht_getHumidity(rht_t *prht)  { return rht03_getHumidity(prht); }
#else
#include "sht1x.h"
static inline void   rht_init(void) { sht1x_init(); }
static inline void   rht_print(const char *data) { sht1x_print(data); }
static inline int8_t rht_poll(rht_t *prht) { return sht1x_poll(prht); }
static inline int8_t rht_getTemperature(rht_t *prht) { return sht1x_getTemperature(prht); }
static inline int8_t rht_getHumidity(rht_t *prht)  { return sht1x_getHumidity(prht); }
#endif

int8_t rht_read(rht_t *rht, int8_t echo);

#ifdef __cplusplus
}
#endif
#endif


