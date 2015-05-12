/*  Simple command line parser for ATmega32

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "pinio.h"
#include "timer.h"
#include "serial.h"
#include "serial_cli.h"

const char *is_on(uint8_t val)
{
	if (val) return "ON ";
	return "OFF";
}

char *get_arg(char *str)
{
	char *arg;

	for(arg = str; *arg && *arg != ' '; arg++);

	if (*arg == ' ') {
		*arg = '\0';
		arg++;
	}

	return arg;
}

static uint16_t led;
static uint8_t  cursor;
static char cmd[CMD_LEN + 1];
static char hist[CMD_LEN + 1];

void cli_init(void)
{
	led = 0;
	cursor = 0;

	for(uint8_t i = 0; i <= CMD_LEN; i++) {
		cmd[i] = '\0';
		hist[i] = '\0';
	}
	uart_puts_p(PSTR("> "));
}

int8_t cli_interact(cli_processor *process, void *ptr)
{
	uint16_t ch;

	// check if LED1 is ON long enough (20 ms in this case)
	if (led) {
		uint16_t span = mill16();
		if ((span - led) > 20) {
			mmr_led_off();
			led = 0;
		}
	}

	if ((ch = serial_getc()) == 0)
		return 0;
	// light up on board LED1 indicating serial data 
	mmr_led_on();
	led = mill16();
	if (!led)
		led = 1;

	if (ch & EXTRA_KEY) {
		if (ch == ARROW_UP && (cursor == 0)) {
			// execute last successful command
			for(cursor = 0; ; cursor++) {
				cmd[cursor] = hist[cursor];
				if (cmd[cursor] == '\0')
					break;
			}
			uart_puts(cmd);
		}
		return 1;
	}

	if (ch == '\n') {
		serial_putc(ch);
		if (*cmd) {
			int8_t ret = process(cmd, ptr);
			memcpy(hist, cmd, sizeof(cmd));
			if (ret == CLI_EARG)
				uart_puts_p(PSTR("Invalid argument\n"));
			else if (ret == CLI_ENOTSUP)
				uart_puts_p(PSTR("Unknown command\n"));
			else if (ret == CLI_ENODEV)
				uart_puts_p(PSTR("Device error\n"));
		}
		for(uint8_t i = 0; i < cursor; i++)
			cmd[i] = '\0';
		cursor = 0;
		serial_putc('>');
		serial_putc(' ');
		return 1;
	}

	// backspace processing
	if (ch == '\b') {
		if (cursor) {
			cursor--;
			cmd[cursor] = '\0';
			serial_putc('\b');
			serial_putc(' ');
			serial_putc('\b');
		}
	}

	// skip control or damaged bytes
	if (ch < ' ')
		return 0;

	// echo
	serial_putc((uint8_t)ch);

	cmd[cursor++] = (uint8_t)ch;
	cursor &= CMD_LEN;
	// clean up in case of overflow (command too long)
	if (!cursor) {
		for(uint8_t i = 0; i <= CMD_LEN; i++)
			cmd[i] = '\0';
	}

	return 1;
}
