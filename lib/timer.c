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

#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#include "pinio.h"
#include "timer.h"

#if (F_CPU != 8000000)
#error Change init_millis() for F_CPU != 8MHz
#endif

static volatile uint8_t ms_clock; // up to 100 ms
volatile uint8_t  tenth_clock;
volatile uint32_t millis_clock;
volatile uint32_t rtc_clock;
volatile uint8_t  rtc_sec, rtc_min, rtc_hour;
volatile uint8_t rtc_wdt, rtc_wdtclock;

// compare interrupt handler
ISR(TIMER1_COMPA_vect)
{
	millis_clock++;
	// separate counter for tenth of a second
	ms_clock++;
	if (ms_clock == 100) {
		tenth_clock++;
		ms_clock = 0;
		// handle our long watchdog timer
		if (rtc_wdt) {
			if (++rtc_wdtclock > rtc_wdt) {
				wdt_enable(WDTO_15MS);
				while (1);
			}
		}
	}
}

ISR(TIMER2_COMP_vect)
{
	rtc_clock++;
	// Increment time
	if (++rtc_sec == 60) {
		rtc_sec = 0;
		if (++rtc_min == 60) {
			rtc_min = 0;
			if (++rtc_hour == 24)
				rtc_hour = 0;
		}
	}
}

// initialize 1ms timer
void init_time_clock(uint8_t clock)
{
	millis_clock = 0;
	tenth_clock  = 0;
	ms_clock     = 0;
	rtc_clock    = 0;
	rtc_sec = rtc_min = rtc_hour = 0;

	if (clock & CLOCK_MILLIS) {
		// for 8MHz: divide by 64 and then increment clock every millisecond
		TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
		OCR1A = 125;
		// enable compare interrupt
		TIMSK |= _BV(OCIE1A);
	}

	// RTC timer
	if (clock & CLOCK_RTC) {
		OCR2 = 31; //
		TCCR2 = _BV(WGM21) | _BV(CS22) | _BV(CS21) | _BV(CS20);
		TIFR = _BV(OCF2);
		TIMSK |= _BV(OCIE2);
		ASSR |= _BV(AS2);
	}

	// PWM timer
	if (clock & CLOCK_PWM) {
		// fast PWM: WGM00 & WGM01 are set
		// not inverted PWM: COM01 is set
		// frequency F_CPU/(256*1): only CS00 is set
		TCCR0 = _BV(WGM00) | _BV(WGM01) | _BV(COM01) | _BV(CS00);
		// set PB3 (OC0) as PWM output
		DDRB |= _BV(PB3);
	}
}

