/* Command line parser for ILI9225 tester

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

#include "pinio.h"
#include "timer.h"
#include "serial.h"
#include "bmfont.h"
#include "ili9225.h"
#include "serial_cli.h"

#include "ili_main.h"

void test_draw(ili9225_t *ili, uint8_t reset);

static const char version[] PROGMEM = "2015-05-04\n";

// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  mem\n"
	"  time\n"
	"  reset\n"
	"  status\n"
	"  calibrate\n"
	"  led on|off\n"
	"  set osccal X\n"
	"  ili dir 0|1\n"
	"  ili led on|off|0-255\n"
	"  ili init|draw|text str\n"
	"  ili disp standby|off|on\n"
	"  ili scroll set start end\n"
	"  ili scroll line\n";

int8_t cli_ili(char *buf, void *ptr)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	ili9225_t *ili = ptr;
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
		if (!(active & ACTIVE_DLED))
			pinMode(PND7, OUTPUT_LOW);
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

	if (str_is(cmd, PSTR("ili"))) {
		char *sval = get_arg(arg);
		if (str_is(arg, PSTR("init"))) {
			ili9225_init(ili);
			return 0;
		}
		if (str_is(arg, PSTR("draw"))) {
			test_draw(ili, 1);
			return 0;
		}
		if (str_is(arg, PSTR("text"))) {
			ili9225_text(ili, 0, 0, sval, TEXT_OVERLINE | TEXT_UNDERLINE);
			return 0;
		}
		if (str_is(arg, PSTR("disp"))) {
			if (str_is(sval, PSTR("on"))) {
				ili9225_set_disp(ili, ILI9225_DISP_ON);
				return 0;
			}
			if (str_is(sval, PSTR("off"))) {
				ili9225_set_disp(ili, ILI9225_DISP_OFF);
				return 0;
			}
			if (str_is(sval, PSTR("standby"))) {
				ili9225_set_disp(ili, ILI9225_DISP_STANDBY);
				return 0;
			}
			return CLI_EARG;
		}
		if (str_is(arg, PSTR("led"))) {
			if (str_is(sval, PSTR("on"))) {
				ili9225_set_backlight(ili, ILI9225_BKL_ON);
				return 0;
			}
			if (str_is(sval, PSTR("off"))) {
				ili9225_set_backlight(ili, ILI9225_BKL_OFF);
				return 0;
			}
			uint8_t duty = atoi(sval);
			ili9225_set_backlight(ili, duty);
			return CLI_EOK;
		}
		if (str_is(arg, PSTR("dir"))) {
			uint8_t dir = atoi(sval);
			ili9225_set_dir(ili, dir);
			test_draw(ili, 1);
			ili9225_text(ili, 0, 0, "text text text text ", TEXT_OVERLINE | TEXT_UNDERLINE);
			return 0;
		}
		if (str_is(arg, PSTR("scroll"))) {
			char *start = get_arg(sval);
			char *end = get_arg(start);
			if (str_is(sval, PSTR("set"))) {
				uint8_t top = atoi(start);
				uint8_t bot = atoi(end);
				if (top > ILI9225_MAX_Y)
					top = ILI9225_MAX_Y;
				if (bot > ILI9225_MAX_Y)
					bot = ILI9225_MAX_Y;
				if (top < bot)
					ili9225_set_scroll(ili, top, bot);
				else
					ili9225_set_scroll(ili, bot, top);
				return 0;
			}
			uint8_t line = atoi(sval);
			if (line < 219 && line) {
				ili9225_set_fg_color(ili, RGB16_RED);
				ili9225_line(ili, 0, line-1, 175, line-1);
			}
			ili9225_scroll(ili, line);
			return 0;
		}
		return CLI_EARG;
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
		return -1;
	}

	return CLI_ENOTSUP;
}
