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
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "i2cmaster.h"

#include "pinio.h"
#include "timer.h"
#include "serial.h"

#include "test_cli.h"
#include "test_main.h"

#define UART_BAUD_RATE 38400LL // 38400 at 8MHz gives only 0.2% errors

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some default variables we want to store in EEPROM
// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = 178;

// node id
uint8_t EEMEM em_nid = 5;

// by default RFM12 uses max TX power
uint8_t EEMEM em_txpwr = 0;

// runtime flags
uint8_t EEMEM em_flags = LOAD_OSCCAL;

uint8_t  flags;
uint8_t  active;
uint32_t uptime;

inline const char *is_on(uint8_t val)
{
	if (val) return "ON";
	return "OFF";
}

uint8_t  nid;   // node id
uint8_t  txpwr; // RFM12 TX power

uint16_t rd_bv;     // battery voltage
uint8_t  rd_ts[3];  // last session time
uint8_t  rd_signal; // session signal
uint16_t rd_arssi;

int main(void)
{
	mmr_led_on(); // turn on LED while booting
	sei();

	// initialise all components
	nid = eeprom_read_byte(&em_nid); // our node id
	txpwr = eeprom_read_byte(&em_txpwr);
	flags = eeprom_read_byte(&em_flags);
	active = 0;
	serial_init(UART_BAUD_RATE);

	if (flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	delay(1000); // to make sure that our 32kHz crystal is up and running
	// setup RTC timer with external 32768 crystal and
	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_RTC | CLOCK_MILLIS);

	// RFM12 nIRQ pin
	_pin_mode(&DDRD, _BV(PD2), INPUT_HIGHZ);

	// interactive mode pin, if grounded then MCU will not enter sleep state
	// and will read serial port for commands and display data on oled screen
	_pin_mode(&DDRB, _BV(PB0), INPUT_UP);
	MCUCSR |= _BV(JTD); // disable JTag if enabled in the fuses

	// reset our soft clock
	uptime = 0;
	active |= UART_ACTIVE;
	flags |= DATA_INIT;

	print_status();
	cli_init();

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

	mmr_led_off();

	for(;;) {
		cli_interact(NULL);
		// once-a-second checks
		if (tenth_clock >= 10) {
			mmr_led_on();
			tenth_clock = 0;
			uptime++;
			mmr_led_off();
		}
	}
}

void get_rtc_time(char *buf)
{
	uint8_t ts[3];
	rtc_get_time(ts);
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), ts[2], ts[1], ts[0]);
}

void print_status(void)
{
	char buf[16];

	printf_P(PSTR("Sensor ID %d, txpwr %ddB\n"), nid, -3*txpwr);
	get_rtc_time(buf);
	printf_P(PSTR("RTC time %s "), buf);
	printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
}

