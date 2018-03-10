/* Command line parser for I/O tester

   Copyright (c) 2018 Andrey Chilikin (https://github.com/achilikin)

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
#include "i2cmem.h"
#include "bmp180.h"
#include "ds18x.h"
#include "serial.h"
#include "serial_cli.h"

#include "test_main.h"

static const char version[] PROGMEM = "2015-12-06\n";

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
	"  set pin (d3,b4,c2...)\n"
	"  bmp init|poll\n"
	"  ds init|get|start|read|data|write\n"
	"  i2cmem init [hex]\n"
	"  i2cmem write addr val\n"
	"  i2cmem dump [addr [len]]\n";

static bmp180_t bmp;
static uint8_t ds_pin = PNB1;

uint8_t strnum(const char *str, uint8_t base)
{
	if (str[0] == 'x' || (str[0] == '0' && str[1] == 'x')) {
		str++;
		if (*str == 'x')
			str++;
		base = 16;
	}
	uint8_t num = (uint8_t)strtoul(str, NULL, base);
	return num;
}

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

	if (str_is(cmd, PSTR("i2cmem"))) {
		char *sval0 = get_arg(arg);
		char *sval1 = get_arg(sval0);
		uint16_t val0 = atoi(sval0);

		if (str_is(arg, PSTR("dump"))) {
			uint16_t val1 = atoi(sval1);
			if (val1 == 0)
				val1 = 256;
			for(uint16_t i = 0; i < val1; i++) {
				uint8_t data;
				if (i2cmem_read_data(val0 + i, &data, 1) != 0)
					return CLI_ENODEV;
				if (!(i % 16) && i)
					uart_puts("\n");
				printf(" %02X", data);
			}
			uart_puts("\n");
			return 0;
		}

		if (str_is(arg, PSTR("write"))) {
			uint8_t data = strnum(sval1, 10);
			if (i2cmem_write_byte(val0, data) == 0)
				return 0;
			return CLI_ENODEV;
		}

		if (str_is(arg, PSTR("init"))) {
			int8_t ret;
			uint8_t line[I2C_MEM_PAGE_SIZE];
			uint8_t fill = strnum(sval0, 16);
			memset(line, fill, sizeof(line));
			uint32_t ms = millis();
			for(uint16_t page = 0; page < I2C_MEM_MAX_PAGE; page++) {
				ret = i2cmem_write_page(page, 0, line, I2C_MEM_PAGE_SIZE);
				if (ret < 0) {
					printf("error writing page %u\n", page);
					ret = CLI_ENODEV;
					break;
				}
			}
			ms = millis() - ms;
			printf("filled in %lums, %lums per page\n", ms, ms/512);
			return ret;
		}
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

	if (str_is(cmd, PSTR("bmp"))) {
		if (str_is(arg, PSTR("init"))) {
			int8_t error = bmp180_init(&bmp);
			if (error)
				printf_P(PSTR("bmp180 init error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("poll"))) {
			int8_t error = bmp180_poll(&bmp, BMP180_T_MODE);
			if (bmp.valid & BMP180_T_VALID) {
				int8_t t = get_u8val(bmp.t);
				printf("t %d.%02u\n", t, bmp.tdec);
			}
			else
				printf_P(PSTR("bmp180 init error %d\n"), error);
			return 0;
		}
		return -1;
	}

	if (str_is(cmd, PSTR("ds"))) {
		if (str_is(arg, PSTR("init"))) {
			int8_t error = ds18x_init(ds_pin, DSx18_TYPE_B);
			if (error)
				printf_P(PSTR("ds1820 init error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("get"))) {
			ds_temp_t t;
			uint8_t buf[DS18x_PAD_LEN];
			int8_t error = ds18x_get_temp(ds_pin, &t, buf);
			for (uint8_t i = 0; i < DS18x_PAD_LEN; i++)
				printf("%02X ", buf[i]);
			printf("%d.%u\n", t.val, t.dec);
			if (error)
				printf_P(PSTR("ds1820 get error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("read"))) {
			ds_temp_t t;
			uint8_t buf[DS18x_PAD_LEN];
			int8_t error = ds18x_read_temp(ds_pin, &t, buf);
			for (uint8_t i = 0; i < DS18x_PAD_LEN; i++)
				printf("%02X ", buf[i]);
			printf("%d.%u\n", t.val, t.dec);
			if (error)
				printf_P(PSTR("ds1820 read error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("start"))) {
			int8_t error = ds18x_cmd(ds_pin, DS18x_CMD_COVERT);
			if (error)
				printf_P(PSTR("ds1820 start error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("data"))) {
			uint16_t data;
			int8_t error = ds18x_read_data(ds_pin, &data);
			printf_P(PSTR("ds1820 data %u\n"), data);
			if (error)
				printf_P(PSTR("ds1820 data error %d\n"), error);
			return 0;
		}
		if (str_is(arg, PSTR("write"))) {
			uint16_t data = uptime;
			int8_t error = ds18x_wite_data(ds_pin, data);
			printf_P(PSTR("ds1820 data %u\n"), data);
			if (error)
				printf_P(PSTR("ds1820 write error %d\n"), error);
			return 0;
		}
		return CLI_EARG;
	}

	return CLI_ENOTSUP;
}
