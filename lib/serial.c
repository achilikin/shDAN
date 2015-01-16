/*  Simple wrappers for reading serial port (similar to getch) on ATmega32L

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
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "serial.h"

// Escape sequence states
#define ESC_CHAR    0
#define ESC_BRACKET 1
#define ESC_BRCHAR  2
#define ESC_TILDA   3
#define ESC_CRLF    5

uint8_t osccal_def;

// -------------------- UART related begin ----------------------------
// USART stability wrt F_CPU clock
// http://www.wormfood.net/avrbaudcalc.php
// At 8MHz and 3.3V we could get a lot of errors with baudrate > 38400
// using calibrate() might help a bit, but better stick to 38400
// 38400 at 8MHz gives only 0.2% errors
// 115200 at 8MHz errors up to 7.8%

int uart_tx(char data, FILE *stream);
FILE uart_stdout = FDEV_SETUP_STREAM(uart_tx, NULL, _FDEV_SETUP_WRITE);

int uart_tx(char data, FILE *stream __attribute__((unused)))
{
	if (data == '\n')
		uart_putc('\r');
	uart_putc(data);
	return 0;
}
// ---------------------- UART related end ----------------------------

// Run it for calibration if you want to use serial at 115200
// Note min/max OSCCAL values you can read serial output
// and use a value close to the middle of the range for em_osccal above
void serial_calibrate(uint8_t osccal)
{
	const uint8_t delta = 20;

	printf_P(PSTR("Probing -+%d of OSCCAL ='%d'\n"), delta, osccal);
	_delay_ms(250);
	for(uint8_t i = osccal - delta; i < (osccal + delta); i++) {
		OSCCAL = i;
		_delay_ms(50);
		if (i == osccal)
			printf_P(PSTR("OSCCAL >>>>'%d'<<<<\n"), i);
		else
			printf_P(PSTR("OSCCAL ----'%d'----\n"), i);
		_delay_ms(200);
	}
	OSCCAL = osccal; // restore default one
	puts_P(PSTR("\nDone"));
}

uint8_t serial_set_osccal(uint8_t osccal)
{
	uint8_t ret = OSCCAL;
	OSCCAL = osccal;
	printf_P(PSTR("Loaded new OSCCAL %d instead of %d\n"), osccal, osccal_def);
	return ret;
}

void serial_init(long baud_rate)
{
	// all necessary initializations
	uart_init(UART_BAUD_SELECT(baud_rate,F_CPU));
	// enable printf, puts...
	stdout = &uart_stdout;
	// save current OSCCAL just in case
	osccal_def = OSCCAL;
}

uint16_t serial_getc(void)
{
	static uint8_t esc = ESC_CHAR;
	static uint8_t idx = 0;
	uint16_t ch;

	/*
     * Get received character from ringbuffer
     * uart_getc() returns in the lower byte the received character and 
     * in the higher byte (bitmask) the last receive error
     * UART_NO_DATA is returned when no data is available.
     *
    */
	ch = uart_getc();
	if (ch & 0xFF00) {
		if (ch & UART_FRAME_ERROR) {
			/* Framing Error detected, i.e no stop bit detected */
            uart_puts_P("UART Frame Error.");
		}
        if (ch & UART_OVERRUN_ERROR) {
			/* 
			* Overrun, a character already present in the UART UDR register was 
			* not read by the interrupt handler before the next character arrived,
			* one or more received characters have been dropped
			*/
			uart_puts_P("UART Overrun Error.");
        }
		if (ch & UART_BUFFER_OVERFLOW) {
			/* 
			* We are not reading the receive buffer fast enough,
			* one or more received character have been dropped 
			*/
			uart_puts_P("Buffer overflow error.");
		}
		return 0;
	}
	
	// ESC sequence state machine
	if (ch == 27) {
		esc = ESC_BRACKET;
		return 0;
	}
	if (esc == ESC_BRACKET) {
		if (ch == '[') {
			esc = ESC_BRCHAR;
			return 0;
		}
	}
	if (esc == ESC_BRCHAR) {
		esc = ESC_CHAR;
		if (ch >= 'A' && ch <= 'D') {
			ch |= EXTRA_KEY;
			return ch;
		}
		if ((ch >= '1') && (ch <= '6')) {
			esc = ESC_TILDA;
			idx = ch - '0';
			return 0;
		}
		return ch;
	}
	if (esc == ESC_TILDA) {
		esc = ESC_CHAR;
		if (ch == '~') {
			ch = EXTRA_KEY | idx;
			return ch;
		}
		return 0;
	}

	// convert CR to LF 
	if (ch == '\r') {
		esc = ESC_CRLF;
		return '\n';
	}
	// do not return LF if it is part of CR+LF combination
	if (ch == '\n') {
		if (esc == ESC_CRLF) {
			esc = ESC_CHAR;
			return 0;
		}
	}
	esc = ESC_CHAR;
	return ch;
}
