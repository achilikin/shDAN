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
#include <stdio.h>
#include <string.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "cli.h"
#include "rht.h"
#include "main.h"
#include "pinio.h"
#include "sht1x.h"
#include "ns741.h"
#include "timer.h"
#include "serial.h"
#include "pcf2127.h"
#include "ossd_i2c.h"

static uint16_t free_mem(void)
{
	extern int __heap_start, *__brkval; 
	unsigned val;
	val = (unsigned)((int)&val - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
	return val;
}

inline const char *is_on(uint8_t val)
{
	if (val) return "ON";
	return "OFF";
}

inline int8_t str_is(const char *cmd, const char *str)
{
	return strcmp_P(cmd, str) == 0;
}

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


// list of supported commands 
const char cmd_list[] PROGMEM = 
	"  mem\n"
	"  poll\n"
	"  reset\n"
	"  status\n"
	"  calibrate\n"
	"  set osccal X\n"
	"  set time HH:MM:SS\n"
	"  set date YY/MM/DD\n"
	"  echo rht|adc|rds|remote|off\n"
	"  rtc time|date|dump [mem]|init [mem]\n"
	"  adc chan\n"
	"  get pin (d3, b4,c2...)\n"
	"  rdsid id\n"
	"  rdstext text\n"
	"  freq nnnn\n"
	"  txpwr 0-3\n"
	"  volume 0-6\n"
	"  mute on|off\n"
	"  stereo on|off\n"
	"  radio on|off\n"
	"  gain low|off\n";

static int8_t process(char *buf, void *rht)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	memcpy(cmd, buf, sizeof(cmd));
	arg = get_arg(cmd);

	if (str_is(cmd, PSTR("help"))) {
		uart_puts_p(cmd_list);
		return 0;
	}

	if (str_is(cmd, PSTR("rtc"))) {
		char *sval = get_arg(arg);
		if (str_is(arg, PSTR("init"))) {
			if (str_is(sval, PSTR("mem"))) {
				for(uint8_t i = 0; i < 16; i++)
					cmd[i] = 0;
				for(uint8_t i = 0; i < PCF_RAM_SIZE/16; i++)
					pcf2127_ram_write(i*16, (uint8_t *)cmd, 16);
				return 0;
			}

			pcf2127_init();
			return 0;
		}

		if (str_is(arg, PSTR("dump"))) {
			if (str_is(sval, PSTR("mem"))) {
				for(uint8_t i = 0; i < PCF_RAM_SIZE/16; i++) {
					pcf2127_ram_read(i*16, (uint8_t *)cmd, 16);
					printf_P(PSTR("%3u | "), i*16);
					for(int8_t n = 0; n < 16; n++)
						printf("%02X ", cmd[n]);
					printf("\n");
				}
				return 0;
			}

			if (pcf2127_read(0x00, (uint8_t *)cmd, PCF_MAX_REG) == 0) {
				for(int8_t i = 0; i < PCF_MAX_REG; i++) {
					printf("%02X ", i);
				}
				printf("\n");
				for(int8_t i = 0; i < PCF_MAX_REG; i++) {
					printf("%02X ", cmd[i]);
				}
				printf("\n");
			}
			return 0;
		}

		if (str_is(arg, PSTR("time"))) {
			uint8_t ts[3];
			if (pcf2127_get_time((pcf_td_t *)ts, 0) == 0) {
				// reset our sw clock
				sw_clock = ts[2];
				sw_clock += ts[1] * 60;
				sw_clock += ts[0] * 3600;
				printf_P(PSTR("%02d:%02d:%02d\n"), ts[0], ts[1], ts[2]);
			}
			return 0;
		}

		if (str_is(arg, PSTR("date"))) {
			pcf_td_t td;
			if (pcf2127_get_date(&td) == 0) {
				printf_P(PSTR("20%02d/%02d/%02d %02d:%02d:%02d\n"),
					td.year, td.month, td.day, td.hour, td.min, td.sec);
			}
			return 0;
		}
		return -1;
	}

	if (str_is(cmd, PSTR("set"))) {
		char *sval = get_arg(arg);

		if (str_is(arg, PSTR("osccal"))) {
			uint8_t osc = atoi(sval);
			serial_set_osccal(osc);
			eeprom_update_byte(&em_osccal, osc);
			return 0;
		}

		if (str_is(sval, PSTR("time"))) {
			uint8_t ts[3];
			ts[0] = strtoul(arg, &arg, 10);
			if (ts[0] < 24 && *arg == ':') {
				ts[1] = strtoul(arg + 1, &arg, 10);
				if (ts[1] < 60 && *arg == ':') {
					ts[2] = strtoul(arg + 1, &arg, 10);
					if (ts[2] < 60) {
						pcf2127_set_time((pcf_td_t *)ts);
						return 0;
					}
				}
			}
		}

		if (str_is(sval, PSTR("date"))) {
			pcf_td_t ts;
			ts.year = strtoul(arg, &arg, 10);
			if (ts.year < 99 && *arg == '/') {
				ts.month = strtoul(arg + 1, &arg, 10);
				if (ts.month < 13 && *arg == '/') {
					ts.day = strtoul(arg + 1, &arg, 10);
					if (ts.day < 31) {
						pcf2127_set_date(&ts);
						return 0;
					}
				}
			}
		}
		return -1;
	}

	if (str_is(cmd, PSTR("reset"))) {
		puts_P(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
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
		rht_read(rht, RHT_ECHO);
		ns741_rds_set_radiotext(rds_data);
		return 0;
	}

	if (str_is(cmd, PSTR("log"))) {
		if (str_is(arg, PSTR("on")))
			rt_flags |= RHT_LOG;
		if (str_is(arg, PSTR("off")))
			rt_flags &= ~RHT_LOG;
		printf_P(PSTR("log is %s\n"), is_on(rt_flags & RHT_LOG));
		return 0;
	}

	if (str_is(cmd, PSTR("echo"))) {
		if (str_is(arg, PSTR("rht"))) {
			rt_flags ^= RHT_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(rt_flags & RHT_ECHO));
			return 0;
		}
		if (str_is(arg, PSTR("remote"))) {
			rt_flags ^= RD_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(rt_flags & RD_ECHO));
			return 0;
		}
#if ADC_MASK
		if (str_is(arg, PSTR("adc"))) {
			rt_flags ^= ADC_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(rt_flags & ADC_ECHO));
			return 0;
		}
#endif
		if (str_is(arg, PSTR("rds"))) {
			ns741_rds_debug(1);
			return 0;
		}
		if (str_is(arg, PSTR("off"))) {
			rt_flags &= ~(ADC_ECHO | RHT_ECHO | RHT_LOG | RD_ECHO);
			printf_P(PSTR("echo OFF\n"));
			return 0;
		}
		return -1;
	}

	if (str_is(cmd, PSTR("mem"))) {
		printf_P(PSTR("memory %d\n"), free_mem());
		return 0;
	}

#if ADC_MASK
	if (str_is(cmd, PSTR("adc"))) {
		uint8_t ai = atoi((const char *)arg);
		if (ai > 7 || (ADC_MASK & (1 << ai)) == 0) {
			printf_P(PSTR("Invalid Analog input, use"));
			for(uint8_t i = 0; i < 8; i++) {
				if (ADC_MASK & (1 << i))
					printf(" %d", i);
			}
			printf("\n");
			return -1;
		}
		uint16_t val = analogRead(ai);
		uint32_t v = val * 323LL + 500LL;
		uint32_t dec = (v % 100000LL) / 1000LL;
		v = v / 100000LL;
		printf_P(PSTR("ADC %d %4d %d.%dV\n"), ai, val, (int)v, (int)dec);
		// if you want to use floats then do not forget to uncomment in Makefile 
		// #PRINTF_LIB = $(PRINTF_LIB_FLOAT)
		// to enable float point printing
		//double v = val*3.3/1024.0;
		//printf("ADC %d %4d %.2fV (%s)\n", ai, val, v);
		return 0;
	}
#endif

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

	if (str_is(cmd, PSTR("rdsid"))) {
		if (*arg != '\0') {
			memset(rds_name, 0, 8);
			for(int8_t i = 0; (i < 8) && arg[i]; i++)
				rds_name[i] = arg[i];
			ns741_rds_set_progname(rds_name);
			eeprom_update_block((const void *)rds_name, (void *)em_rds_name, 8);
		}
		printf_P(PSTR("%s %s\n"), cmd, rds_name);
		ossd_putlx(0, -1, rds_name, 0);
		return 0;
	}

	if (str_is(cmd, PSTR("rdstext"))) {
		if (*arg == '\0') {
			puts(rds_data);
			return 0;
		}
		ns_rt_flags |= RDS_RESET;
		if (str_is(arg, PSTR("reset"))) 
			ns_rt_flags &= ~RDS_RT_SET;
		else {
			ns_rt_flags |= RDS_RT_SET;
			ns741_rds_set_radiotext(arg);
		}
		return 0;
	}

	if (str_is(cmd, PSTR("status"))) {
		printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
		printf_P(PSTR("RDSID %s, %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
			rds_name, fm_freq,
			is_on(ns_pwr_flags & NS741_POWER), is_on(ns_rt_flags & NS741_STEREO),
			ns_pwr_flags & NS741_TXPWR, (ns_pwr_flags & NS741_VOLUME) >> 8, (ns_pwr_flags & NS741_GAIN) ? -9 : 0);
		printf_P(PSTR("%s %s\n"), rds_data, hpa);
		print_rd();
		return 0;
	}

	if (str_is(cmd, PSTR("freq"))) {
		uint16_t freq = atoi((const char *)arg);
		if (freq < NS741_MIN_FREQ || freq > NS741_MAX_FREQ) {
			puts_P(PSTR("Frequency is out of band\n"));
			return -1;
		}
		freq = NS741_FREQ_STEP*(freq / NS741_FREQ_STEP);
		if (freq != radio_freq) {
			radio_freq = freq;
			ns741_set_frequency(radio_freq);
			eeprom_update_word(&em_radio_freq, radio_freq);
		}
		printf_P(PSTR("%s set to %u\n"), cmd, radio_freq);
		sprintf_P(fm_freq, PSTR("FM %u.%02uMHz"), radio_freq/100, radio_freq%100);
		ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
		return 0;
	}

	if (str_is(cmd, PSTR("txpwr"))) {
		uint8_t pwr = atoi((const char *)arg);
		if (pwr > 3) {
			puts_P(PSTR("Invalid TX power level\n"));
			return -1;
		}
		ns_pwr_flags &= ~NS741_TXPWR;
		ns_pwr_flags |= pwr;
		ns741_txpwr(pwr);
		printf_P(PSTR("%s set to %d\n"), cmd, pwr);
		eeprom_update_byte(&em_ns_pwr_flags, ns_pwr_flags);

		get_tx_pwr(status);
		uint8_t font = ossd_select_font(OSSD_FONT_6x8);
		ossd_putlx(7, -1, status, 0);
		ossd_select_font(font);
		return 0;
	}

	if (str_is(cmd, PSTR("volume"))) {
		uint8_t gain = (ns_pwr_flags & NS741_VOLUME) >> 4;

		if (*arg != '\0')
			gain = atoi((const char *)arg);
		if (gain > 6) {
			puts_P(PSTR("Invalid Audio Gain value 0-6\n"));
			return -1;
		}
		ns741_volume(gain);
		printf_P(PSTR("%s set to %d\n"), cmd, gain);
		ns_pwr_flags &= ~NS741_VOLUME;
		ns_pwr_flags |= gain << 4;
		eeprom_update_byte(&em_ns_pwr_flags, ns_pwr_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("gain"))) {
		int8_t gain = (ns_pwr_flags & NS741_GAIN) ? -9 : 0;

		if (str_is(arg, PSTR("low")))
			gain = -9;
		else if (str_is(arg, PSTR("off")))
			gain = 0;

		ns741_gain(gain);
		printf_P(PSTR("%s is %ddB\n"), cmd, gain);
		ns_pwr_flags &= ~NS741_GAIN;
		if (gain)
			ns_pwr_flags |= NS741_GAIN;
		eeprom_update_byte(&em_ns_pwr_flags, ns_pwr_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("mute"))) {
		if (str_is(arg, PSTR("on")))
			ns_rt_flags |= NS741_MUTE;
		else if (str_is(arg, PSTR("off")))
			ns_rt_flags &= ~NS741_MUTE;
		ns741_mute(ns_rt_flags & NS741_MUTE);
		printf_P(PSTR("mute %s\n"), is_on(ns_rt_flags & NS741_MUTE));
		return 0;
	}

	if (str_is(cmd, PSTR("stereo"))) {
		if (str_is(arg, PSTR("on")))
			ns_rt_flags |= NS741_STEREO;
		else if (str_is(arg, PSTR("off")))
			ns_rt_flags &= ~NS741_STEREO;
		ns741_stereo(ns_rt_flags & NS741_STEREO);
		printf_P(PSTR("stereo %s\n"), is_on(ns_rt_flags & NS741_STEREO));
		eeprom_update_byte(&em_ns_rt_flags, ns_rt_flags);
		return 0;
	}

	if (str_is(cmd, PSTR("radio"))) {
		if (str_is(arg, PSTR("on")))
			ns_pwr_flags |= NS741_POWER;
		else if (str_is(arg, PSTR("off")))
			ns_pwr_flags &= ~NS741_POWER;
		ns741_radio_power(ns_pwr_flags & NS741_POWER);
		printf_P(PSTR("radio %s\n"), is_on(ns_pwr_flags & NS741_POWER));
		get_tx_pwr(status);
		uint8_t font = ossd_select_font(OSSD_FONT_6x8);
		ossd_putlx(7, -1, status, 0);
		ossd_select_font(font);
		return 0;
	}

	printf_P(PSTR("Unknown command '%s'\n"), cmd);
	return -1;
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
}

int8_t cli_interact(void *rht)
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
			if (process(cmd, &rht) == 0)
				memcpy(hist, cmd, sizeof(cmd));
			else
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
