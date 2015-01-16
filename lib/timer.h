/* Simple timer routine for ATmega32 on MMR-70
   Counts milliseconds and separately tenth of a second

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
#ifndef ATMEGA_TIMERS_H
#define ATMEGA_TIMERS_H

#include <util/atomic.h>
#include <avr/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint8_t  tenth_clock;  // increments every 1/10 of a second
extern volatile uint32_t millis_clock; // global millis counter
extern volatile uint32_t rtc_clock;    // rtc driven seconds counter
extern volatile uint8_t  rtc_sec, rtc_min, rtc_hour;

#define CLOCK_RTC    0x01
#define CLOCK_MILLIS 0x02

void init_time_clock(uint8_t clock);

static inline uint32_t millis(void)
{
	uint32_t mil;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	mil = millis_clock;
	}
	return mil;
}

static inline uint16_t mill16(void)
{
	uint16_t mil;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	mil = (uint16_t)millis_clock;
	}
	return mil;
}

static inline uint8_t mill8(void)
{
	uint8_t mil;
	volatile uint8_t *pms = (volatile uint8_t *)&millis_clock;
	mil = pms[0];
	return mil;
}

static inline uint32_t rtc_get_clock(void)
{
	uint32_t rtc;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
	rtc = rtc_clock;
	}
	return rtc;
}

static inline void rtc_get_time(uint8_t *rtc)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		rtc[0] = rtc_sec;
		rtc[1] = rtc_min;
		rtc[2] = rtc_hour;
	}
}

#ifdef __cplusplus
}
#endif

#endif

