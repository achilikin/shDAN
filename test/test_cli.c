/* Command line parser for I/O tester

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

#include "test_main.h"

static const char version[] PROGMEM = "2015-04-14\n";

// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  mem\n"
	"  time\n"
	"  reset\n"
	"  status\n"
	"  calibrate\n"
	"  led on|off\n"
	"  set osccal X\n"
	"  set time hh:mm:ss\n"
	"  adc chan\n"
	"  get pin (d3,b4,c2...)\n"
	"  set pin (d3,b4,c2...)\n";

int8_t cli_test(char *buf, void *ptr)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	ptr = ptr; // unused
	memcpy(cmd, buf, sizeof(cmd));
	arg = get_arg(cmd);

	if (str_is(cmd, PSTR("help"))) {
		uart_puts_p(PSTR("  version: "));
		uart_puts_p(version);
		uart_puts_p(cmd_list);
		return 0;
	}

	// SW reset
	if (str_is(cmd, PSTR("reset"))) {
		puts_P(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
	}

	if (str_is(cmd, PSTR("mem"))) {
		printf_P(PSTR("memory %d\n"), free_mem());
		return 0;
	}

	if (str_is(cmd, PSTR("time"))) {
		get_time(cmd);
		printf_P(PSTR("%s\n"), cmd);
		return 0;
	}

	if (str_is(cmd, PSTR("led"))) {
		if (str_is(arg, PSTR("on")))
			active |= ACTIVE_DLED;
		if (str_is(arg, PSTR("off")))
			active &= ~ACTIVE_DLED;
		printf_P(PSTR("%s is %s\n"), cmd, is_on(active & ACTIVE_DLED));
		return 0;
	}

	if (str_is(cmd, PSTR("status"))) {
		print_status();
		return 0;
	}

	if (str_is(cmd, PSTR("calibrate"))) {
		puts_P(PSTR("\ncalibrating..."));
		if (str_is(arg, PSTR("default")))
			serial_calibrate(osccal_def);
		else
			serial_calibrate(OSCCAL);
		return 0;
	}

	if (str_is(cmd, PSTR("set"))) {
		char *sval = get_arg(arg);
		if (str_is(arg, PSTR("osccal"))) {
			uint8_t osc = atoi(sval);
			serial_set_osccal(osc);
			eeprom_update_byte(&em_osccal, osc);
			return 0;
		}
		
		if (str_is(arg, PSTR("time"))) {
			uint8_t hour, min, sec;
			hour = strtoul(sval, &arg, 10);
			if (hour < 24 && *arg == ':') {
				min = strtoul(arg + 1, &arg, 10);
				if (min < 60 && *arg == ':') {
					sec = strtoul(arg + 1, &arg, 10);
					if (sec < 60) {
						swtime = hour * 3600;
						swtime += min*60;
						swtime += sec;
						return 0;
					}
				}
			}
		}

		if (str_is(arg, PSTR("pin"))) {
			char   port = sval[0];
			uint8_t idx = atoi(sval+1) & 0x07;
			uint8_t val = (atoi(sval+2) & 0x01) << 1;
			if (port == 'b')
				_pin_mode(&DDRB, _BV(idx), OUTPUT | val);
			else
			if (port == 'c')
				_pin_mode(&DDRC, _BV(idx), OUTPUT | val);
			else
			if (port == 'd')
				_pin_mode(&DDRD, _BV(idx), OUTPUT | val);
			else
				return -1;
			printf_P(PSTR("%c%d = %d\n"), port, idx, !!val);
			return 0;
		}

		return -1;
	}

	if (str_is(cmd, PSTR("adc"))) {
		uint8_t ai = atoi((const char *)arg);
		if (ai < 8) {
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
		if (port == 'b') {
			_pin_mode(&DDRB, _BV(idx), INPUT_UP);
			val++;
			val = _pin_get(&PINB, _BV(idx));
		}
		else
		if (port == 'c') {
			_pin_mode(&DDRC, _BV(idx), INPUT_UP);
			val++;
			val = _pin_get(&PINC, _BV(idx));
		}
		else
		if (port == 'd') {
			_pin_mode(&DDRD, _BV(idx), INPUT_UP);
			val++;
			val = _pin_get(&PIND, _BV(idx));
		}
		else
			return -1;
		printf_P(PSTR("%c%d = %d\n"), port, idx, !!val);
		return 0;
	}

	return -2;
}
