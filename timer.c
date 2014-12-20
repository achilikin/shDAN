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

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "mmr70pin.h"

#if (F_CPU != 8000000)
#error Change init_millis() for F_CPU != 8MHz
#endif

static volatile uint8_t sec_clock = 0;

volatile uint32_t ms_clock = 0;
volatile uint8_t tenth_clock = 0;

// compare interrupt handler
ISR(TIMER1_COMPA_vect)
{
   ms_clock++;
   // separate counter for tenth of a second
   sec_clock++;
   if (sec_clock == 100) {
		tenth_clock++;
		sec_clock = 0;
   }
}

// RTC sync vector
ISR(INT1_vect)
{
}

// initialize 1ms timer
void init_time_clock(void)
{
	// for 8MHz: divide by 64 and then increment clock every millisecond
	TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
	OCR1A = 125;
	// enable compare interrupt
	TIMSK |= _BV(OCIE1A);
	
	// set PD3 (INT1) as input and enable pull-up resistor 
	_pin_mode(&DDRD, _BV(PD3), INPUT_UP);
	// enable INT1
	GICR = _BV(INT1);
	// trigger INT1 on falling edge
	MCUCR = _BV(ISC11);
}
