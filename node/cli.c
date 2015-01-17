/* Simple command line parser for ATmega32 on MMR-70

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

#include "cli.h"
#include "main.h"
#include "pinio.h"
#include "timer.h"
#include "serial.h"
#include "rfm12bs.h"
#include "ossd_i2c.h"

static uint16_t free_mem(void)
{
	extern int __heap_start, *__brkval; 
	unsigned val;
	val = (unsigned)((int)&val - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
	return val;
}

inline int8_t str_is(const char *cmd, const char *str)
{
	return strcmp_P(cmd, str) == 0;
}

inline const char *is_on(uint8_t val)
{
	if (val) return "ON";
	return "OFF";
}

// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  reset\n"
	"  status\n"
	"  mem\n"
	"  poll\n"
	"  echo lsd|rsd|off\n"
	"  time\n"
	"  set nid N\n"
	"  set osccal X\n"
	"  set txpwr pwr\n"
	"  set led on|off\n"
	"  set rtc hh:mm:ss\n"
	"  adc chan\n"
	"  get pin (d3,b4,c2...)\n"
	"  calibrate\n";

static char *get_arg(char *str)
{
	char *arg;

	for(arg = str; *arg && *arg != ' '; arg++);

	if (*arg == ' ') {
		*arg = '\0';
		arg++;
	}

	return arg;
}

static int8_t process(char *buf, void *ptr)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	ptr = ptr; // unused
	memcpy(cmd, buf, sizeof(cmd));
	arg = get_arg(cmd);

	if (str_is(cmd, PSTR("help"))) {
		uart_puts_p(cmd_list);
		return 0;
	}

	// SW reset
	if (str_is(cmd, PSTR("reset"))) {
		puts_P(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
	}

	if (str_is(cmd, PSTR("time"))) {
		get_rtc_time(cmd);
		printf_P(PSTR("%s\n"), buf);
		return 0;
	}

	if (str_is(cmd, PSTR("set"))) {
		char *sval = get_arg(arg);
		if (str_is(arg, PSTR("txpwr"))) {
			uint8_t pwr = atoi(sval);
			if (rfm12_set_txpwr(pwr) == 0) {
				txpwr = pwr;
				eeprom_update_byte(&em_txpwr, txpwr);
				return 0;
			}
			return -1;
		}

		if (str_is(arg, PSTR("led"))) {
			if (str_is(sval, PSTR("on")))
				active |= DLED_ACTIVE;
			if (str_is(sval, PSTR("off")))
				active &= ~DLED_ACTIVE;
			printf_P(PSTR("%s is %s\n"), arg, is_on(active & DLED_ACTIVE));
			return 0;
		}

		if (str_is(arg, PSTR("osccal"))) {
			uint8_t osc = atoi(sval);
			serial_set_osccal(osc);
			eeprom_update_byte(&em_osccal, osc);
			return 0;
		}
		
		if (str_is(arg, PSTR("nid"))) {
			uint8_t val = atoi(sval);
			if (!val || val > 0x0F) // sensor id cannot be 0 or > 15
				return -1;
			nid = val;
			eeprom_update_byte(&em_nid, val);
			return 0;
		}

		if (str_is(arg, PSTR("rtc"))) {
			uint8_t hour, min, sec;
			hour = strtoul(sval, &arg, 10);
			if (hour < 24 && *arg == ':') {
				min = strtoul(arg + 1, &arg, 10);
				if (min < 60 && *arg == ':') {
					sec = strtoul(arg + 1, &arg, 10);
					if (sec < 60) {
						cli();
						rtc_hour = hour;
						rtc_min  = min;
						rtc_sec  = sec;
						sei();
						return 0;
					}
				}
			}
		}

		return -1;
	}

	if (str_is(cmd, PSTR("calibrate"))) {
		puts_P(PSTR("\ncalibrating..."));
		if (str_is(arg, PSTR("default")))
			serial_calibrate(osccal_def);
		else
			serial_calibrate(OSCCAL);
		return 0;
	}

	if (str_is(cmd, PSTR("poll"))) {
		puts_P(PSTR("polling..."));
		flags |= DATA_POLL;
		return 0;
	}

	if (str_is(cmd, PSTR("echo"))) {
		if (str_is(arg, PSTR("lsd"))) {
			flags ^= LSD_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(flags & LSD_ECHO));
			return 0;
		}
		if (str_is(arg, PSTR("rsd"))) {
			flags ^= RSD_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(flags & RSD_ECHO));
			return 0;
		}
		if (str_is(arg, PSTR("off"))) {
			flags = 0;
			printf_P(PSTR("echo OFF\n"));
			return 0;
		}
		return -1;
	}

	if (str_is(cmd, PSTR("mem"))) {
		printf_P(PSTR("memory %d\n"), free_mem());
		return 0;
	}

	if (str_is(cmd, PSTR("adc"))) {
		uint8_t ai = atoi((const char *)arg);
		if (ai < 8) {
			// enable ADC in case if we out of sleep mode
			analogReference(VREF_AVCC);
			uint16_t val = analogRead(ai);
			printf_P(PSTR("ADC %d %4d\n"), ai, val);
			return 0;
		}
		return -1;
	}

	if (str_is(cmd, PSTR("get"))) {
		char   port = arg[0];
		uint8_t idx = atoi(arg+1) & 0x07;
		uint8_t val = 0;
		if (port == 'b')
			val = _pin_get(&PINB, 1 << idx);
		if (port == 'c')
			val = _pin_get(&PINC, 1 << idx);
		if (port == 'd')
			val = _pin_get(&PIND, 1 << idx);
		printf_P(PSTR("%c%d = %d\n"), port, idx, !!val);
		return 0;
	}

	if (str_is(cmd, PSTR("status"))) {
		print_status();
		return 0;
	}

	printf_P(PSTR("Unknown command '%s'\n"), cmd);
	return -2;
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

int8_t cli_interact(void *ptr)
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
			int8_t ret = process(cmd, &ptr);
			if (ret == 0)
				memcpy(hist, cmd, sizeof(cmd));
			else if (ret == -1)
				puts_P(PSTR("Invalid format"));
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
