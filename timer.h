/* Simple timer routine for ATmega32 on MMR-70
   Counts milliseconds and separately tenth of a second

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
#ifndef _TIMER_MILLIS_H_
#define _TIMER_MILLIS_H_

#include <avr/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint8_t tenth_clock;
extern volatile uint32_t ms_clock;

void init_millis(void);

static inline uint32_t millis(void)
{
	cli();
	uint32_t mil = ms_clock;
	sei();
	return mil;
}

static inline uint16_t mill16(void)
{
	cli();
	uint16_t mil = (uint16_t)ms_clock;
	sei();
	return mil;
}

static inline uint8_t mill8(void)
{
	cli();
	uint8_t mil = (uint8_t)ms_clock;
	sei();
	return mil;
}

#ifdef __cplusplus
}
#endif

#endif

