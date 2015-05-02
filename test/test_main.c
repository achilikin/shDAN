/* Example of using ATmega32 on MMR-70

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
#include <string.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "pinio.h"
#include "timer.h"
#include "serial.h"
#include "serial_cli.h"

#include "test_main.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

#ifndef DEF_OSCCAL
#define DEF_OSCCAL 182 // average value from serial_calibrate()
#endif

// some default variables we want to store in EEPROM
// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = DEF_OSCCAL;

// runtime flags
uint8_t EEMEM em_flags = LOAD_OSCCAL;

uint8_t  active;
uint8_t  rt_flags;
uint32_t uptime;
uint32_t swtime;

#define CLOCK_TYPE CLOCK_MILLIS

int main(void)
{
	mmr_led_on(); // turn on LED while booting
	sei();

	// initialise all components
	rt_flags = eeprom_read_byte(&em_flags);
	serial_init(UART_BAUD_RATE);

	if (rt_flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	// if 32kHz crystal is attached make sure RTC is up and running 
	if (CLOCK_TYPE & CLOCK_RTC)
		delay(1000);

	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_TYPE);

	// reset our soft clock
	uptime = swtime = 0;

	print_status();
	cli_init();

	analogReference(VREF_AVCC);
	_pin_mode(&DDRB, _BV(PB0), INPUT_UP);
	_pin_mode(&DDRB, _BV(PB1), INPUT_UP);
	_pin_mode(&DDRB, _BV(PB2), INPUT_UP);
	_pin_mode(&DDRB, _BV(PB4), INPUT_UP);
	_pin_mode(&DDRC, _BV(PC0), INPUT_UP);
	_pin_mode(&DDRC, _BV(PC1), INPUT_UP);
	_pin_mode(&DDRC, _BV(PC3), INPUT_UP);
	_pin_mode(&DDRC, _BV(PC4), INPUT_UP);
	_pin_mode(&DDRD, _BV(PD2), INPUT_UP);
	_pin_mode(&DDRD, _BV(PD3), INPUT_UP);

	// alternative to mmr_led_off()
	pinMode(PND7, OUTPUT_HIGH);

	uint8_t led = 0;
	for(;;) {
		cli_interact(cli_test, NULL);
		// once-a-second checks
		if (tenth_clock >= 10) {
			led ^= 0x02;
			// instead of mmr_led_on()/mmr_led_off() 
			pinMode(PND7, OUTPUT | led);
			tenth_clock = 0;
			uptime++;
			swtime++;
			if (swtime == 86400)
				swtime = 0;
		}
	}
}

void get_time(char *buf)
{
	uint8_t ts[3];
	ts[2] = uptime / 3600;
	ts[1] = (uptime / 60) % 60;
	ts[0] = uptime % 60;
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), ts[2], ts[1], ts[0]);
}

void print_status(void)
{
	char buf[16];
	get_time(buf);
	printf_P(PSTR("SW time %s "), buf);
	printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
}

