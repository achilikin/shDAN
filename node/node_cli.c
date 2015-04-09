/* Command line parser for data node

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
#include "rfm12bs.h"
#include "ossd_i2c.h"
#include "serial_cli.h"

#include "node_main.h"

static const char version[] PROGMEM = "2015-04-09\n";
static const char *pstr_eol = version + 10;

// some PROGMEM strings pooling
static const char pstr_on[] PROGMEM = "on";
static const char pstr_off[] PROGMEM = "off";
static const char pstr_echo[] PROGMEM = "%s echo %s\n";

// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  time\n"
	"  reset\n"
	"  status\n"
	"  calibrate\n"
	"  set nid N\n"
	"  set tsync N\n"
	"  set osccal X\n"
	"  set txpwr pwr\n"
	"  set led on|off\n"
	"  set rtc hh:mm:ss\n"
	"  poll\n"
	"  get pin (d3,b4,c2...)\n"
	"  adc chan\n"
	"  mem\n"
	"  echo rx|lsd|off\n";

int8_t cli_node(char *buf, void *ptr)
{
	char *arg;
	char cmd[CMD_LEN + 1];

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
		uart_puts_p(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
	}

	if (str_is(cmd, PSTR("time"))) {
		get_rtc_time(cmd);
		uart_puts(cmd);
		uart_puts_p(pstr_eol);
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
			if (str_is(sval, pstr_on))
				active |= DLED_ACTIVE;
			if (str_is(sval, pstr_off))
				active &= ~DLED_ACTIVE;
			uart_puts(arg);
			uart_puts_p(PSTR(" is "));
			uart_puts(is_on(active & DLED_ACTIVE));
			uart_puts_p(pstr_eol);
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

		if (str_is(arg, PSTR("tsync"))) {
			uint8_t val = atoi(sval);
			if (!val) // sync interval cannot be  0 
				return -1;
			tsync = val;
			eeprom_update_byte(&em_tsync, val);
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
		uart_puts_p(PSTR("\ncalibrating..."));
		if (str_is(arg, PSTR("default")))
			serial_calibrate(osccal_def);
		else
			serial_calibrate(OSCCAL);
		return 0;
	}

	if (str_is(cmd, PSTR("poll"))) {
		uart_puts_p(PSTR("polling..."));
		rt_flags |= RT_DATA_POLL;
		return 0;
	}

	if (str_is(cmd, PSTR("echo"))) {
		if (str_is(arg, PSTR("rx"))) {
			rt_flags ^= RT_RX_ECHO;
			printf_P(pstr_echo, arg, is_on(rt_flags & RT_RX_ECHO));
			return 0;
		}
		if (str_is(arg, PSTR("lsd"))) {
			rt_flags ^= RT_LSD_ECHO;
			printf_P(pstr_echo, arg, is_on(rt_flags & RT_LSD_ECHO));
			return 0;
		}
		if (str_is(arg, pstr_off)) {
			rt_flags = 0;
			uart_puts_p(PSTR("echo OFF\n"));
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
		print_status(ptr);
		return 0;
	}

	return -2;
}
