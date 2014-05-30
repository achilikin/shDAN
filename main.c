/* Example of using ATmega32 on MMR-70

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
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "uart.h" 
#include "i2cmaster.h"

#include "ns741.h"
#include "mmr70pin.h"
#include "rht03.h"
#include "timer.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// support for arrow keys for very simple one command deep history
#define ARROW_UP    0x0100
#define ARROW_DOWN  0x0200
#define ARROW_RIGHT 0x0300
#define ARROW_LEFT  0x0400

unsigned int get_char(void);
int8_t process(char *cmd, rht03_t *rht);

// runtime flags
#define NS741_TXPWR0 0x0000
#define NS741_TXPWR1 0x0001
#define NS741_TXPWR2 0x0002
#define NS741_TXPWR3 0x0003 // txpwr mask
#define NS741_TXPWR  0x0003
#define NS741_RADIO  0x0004
#define NS741_RDS    0x0008
#define NS741_MUTE   0x0010
#define NS741_STEREO 0x0020
#define NS741_GAIN   0x0040 // turn on -9bD audio gain
#define NS741_VOLUME 0x0F00 // volume mask

#define RDS_RESET    0x4000
#define RDS_RT_SET   0x8000

// debug flags
#define ADC_ECHO   0x01
#define RHT03_ECHO 0x02

// define mask of populated ADC inputs here, 0 if not populated
#define ADC_MASK 0xF0

// average value from calibrate() below
// for my copy of MMR70 it is 168 for 115200, 181 for 38400
#define LOAD_OSCCAL 0
uint8_t EEMEM em_osccal = 181;
uint8_t osccal_def;

// some NS741 default variables we want to store in EEPROM
uint8_t  EEMEM em_ns741_name[8] = "MMR70mod"; // NS471 "station" name
uint16_t EEMEM em_ns741_freq  = 9700; // 97.00 MHz
uint16_t EEMEM em_ns741_flags = (NS741_STEREO | NS741_RDS | NS741_TXPWR0);

char ns741_name[9]; // RDS PS name
char rds_data[61];  // RDS RT string

uint16_t ns741_freq;
uint16_t rt_flags = 0;
uint8_t  debug_flags = 0;

// -------------------- UART related begin ----------------------------
// USART stability wrt F_CPU clock
// http://www.wormfood.net/avrbaudcalc.php
// At 8MHz and 3.3V we could get a lot of errors with baudrate > 38400
// using calibrate() might help a bit, but better stick to 38400
#define UART_BAUD_RATE 38400LL // 38400 at 8MHz gives only 0.2% errors
// #define UART_BAUD_RATE 115200UL // at 8MHz errors up to 7.8%

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

inline const char *is_on(bool val)
{
	if (val) return "ON";
	return "OFF";
}

// pretty accurate conversion to 3.3V without using floats 
inline uint8_t get_voltage(uint16_t adc, uint8_t *decimal)
{
	uint32_t v = adc * 323LL + 500LL;
	uint32_t dec = (v % 100000LL) / 1000LL;
	v = v / 100000LL;
	
	*decimal = (uint8_t)dec;
	return (uint8_t)v;
}

// Run it for calibration if you want to use serial at 115200
// Note min/max OSCCAL values you can read serial output
// and use a value close to the middle of the range for em_osccal above
void calibrate(uint8_t osccal)
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

// poll RHT03 and update RDS radio text
bool rht_read(rht03_t *rht, bool echo)
{
	mmr_tp4_set(HIGH);
	const uint8_t *buf;
	bool ok = rht_poll(rht, &buf);
	if (ok) {
		uint8_t tdecimal, hdecimal;
		int8_t tval = rht_getTemperature(rht, &tdecimal);
		int8_t hval = rht_getHumidity(rht, &hdecimal);
		sprintf(rds_data, "T %d.%d H %d.%d", tval, tdecimal, hval, hdecimal);
	}
	if (echo) {
		printf("%u %lu rht %s: ", rht->errors, millis(), ok ? "ok" : "error");
		for(uint8_t i = 0; i < 41; i++)
			printf("%u ", buf[i]);
		printf("| ");
		for(uint8_t i = 0; i < 5; i++)
			printf("%02x ", buf[i+41]);
		printf("| ");
		if (ok)
			printf("%s", rds_data);
		printf("\n");
	}
	mmr_tp4_set(LOW);
	return ok;
}

#define CMD_LEN 0x07F // big enough to accommodate RDS text

int main(void)
{
    uint16_t ch;
	uint16_t led = 0;
	rht03_t  rht;
	char     cmd[CMD_LEN + 1];
	char     hist[CMD_LEN + 1];
	uint8_t  cursor = 0;
	
	for(uint8_t i = 0; i <= CMD_LEN; i++) {
		cmd[i] = '\0';
		hist[i] = '\0';
	}

	// all necessary initializations
    uart_init(UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU));
	// enable printf, puts...
    stdout = &uart_stdout;

	// load new OSCCAL if needed
	osccal_def = OSCCAL;
#if LOAD_OSCCAL
	uint8_t new_osccal = eeprom_read_byte(&em_osccal);
	OSCCAL = new_osccal;
#endif	
	// setup RHT03 sensor, hardcoded to TP5
	rht_init(&rht);
	// setup out ~millisecond timer for mill*() and tenth_clock counter
	init_millis();
	i2c_init(); // needed for ns741_*()

	// initialize NS741 chip	
	eeprom_read_block((void *)ns741_name, (const void *)em_ns741_name, 8);
	ns741_rds_set_progname(ns741_name);
	ns741_rds_set_radiotext("MMR-70 as Temperature and Humidity sensor station");

	// initialize ns741 with default parameters
	ns741_init();
	// read frequency from EEPROM
	ns741_freq = eeprom_read_word(&em_ns741_freq);
	if (ns741_freq < NS741_MIN_FREQ) ns741_freq = NS741_MIN_FREQ;
	if (ns741_freq > NS741_MAX_FREQ) ns741_freq = NS741_MAX_FREQ;
	ns741_set_frequency(ns741_freq);
	// read settings from EEPROM
	uint16_t ns741_word = eeprom_read_word(&em_ns741_flags);
	rt_flags = ns741_word;
	ns741_txpwr(ns741_word & NS741_TXPWR);
	ns741_stereo(ns741_word & NS741_STEREO);
	ns741_input_low(ns741_word & NS741_GAIN);
	ns741_volume((ns741_word & NS741_VOLUME) >> 8);
	// turn ON
	rt_flags |= NS741_RADIO;
	ns741_radio(1);

	sei();

#if _DEBUG
	{
		uint16_t ms = mill16();
		delay(1000);
		ms = mill16() - ms;
		printf_P(PSTR("delay(1000) = %u\n"), ms);
		// calibrate(osccal_def);
#if LOAD_OSCCAL
		printf_P(PSTR("Loading new OSCCAL %d instead of %d\n"), new_osccal, osccal_def);
#endif
	}
#endif

	printf_P(PSTR("ID %s, FM %u.%02uMHz\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
		ns741_name,	ns741_freq/100, ns741_freq%100,
		is_on(rt_flags & NS741_RADIO), is_on(rt_flags & NS741_STEREO),
		rt_flags & NS741_TXPWR, (rt_flags & NS741_VOLUME) >> 8, (rt_flags & NS741_GAIN) ? -9 : 0);

	rht_read(&rht, debug_flags & RHT03_ECHO);
	mmr_led_off();
	mmr_tp4_mode(OUTPUT_LOW); // RHT03 poll LED
	mmr_rdsint_mode(INPUT_HIGHZ);

	// turn on RDS
	ns741_rds(1);
	rt_flags |= NS741_RDS;
	rt_flags |= RDS_RESET; // set reset flag so next poll of RHT will start new text

    for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();

		// poll RHT every 3 seconds (or 30 tenth_clock)
		if (tenth_clock >= 30) {
			tenth_clock = 0;
			rht_read(&rht, debug_flags & RHT03_ECHO);

			if (rt_flags & RDS_RESET) {
				ns741_rds_reset_radiotext();
				rt_flags &= ~RDS_RESET;
			}
			if (!(rt_flags & RDS_RT_SET))
				ns741_rds_set_radiotext(rds_data);
#if ADC_MASK
			if (debug_flags & ADC_ECHO) {
				// if you want to use floats enable PRINTF_LIB_FLOAT in the makefile
				puts("");
				for(uint8_t i = 0; i < 8; i++) {
					if (ADC_MASK & (1 << i)) {
						uint16_t val = analogRead(i);
						uint8_t dec;
						uint8_t v = get_voltage(val, &dec);
						printf("adc%d %4d %d.%02d\n", i, val, v, dec);
					}
				}
			}
#endif
		}

		// check if LED1 is ON long enough (20 ms in this case)
		if (led) {
			uint16_t span = mill16();
			if ((span - led) > 20) {
				mmr_led_off();
				led = 0;
			}
		}

		if ((ch = get_char()) == 0)
			continue;
		// light up on board LED1 indicating serial data 
		mmr_led_on();
		led = mill16();
		if (!led)
			led = 1;

		if (ch == ARROW_UP) {
			// execute last successful command
			for(cursor = 0; ; cursor++) {
				cmd[cursor] = hist[cursor];
				if (cmd[cursor] == '\0')
					break;
			}
			uart_puts(cmd);
			continue;
		}

		if (ch == '\n' && *cmd) {
			if (process(cmd, &rht) == 0)
				memcpy(hist, cmd, sizeof(cmd));
			for(uint8_t i = 0; i < cursor; i++)
				cmd[i] = '\0';
			cursor = 0;
		}
		// backspace processing
		if (ch == '\b') {
			if (cursor) {
				cursor--;
				cmd[cursor] = '\0';
				uart_putc(' ');
				uart_putc('\b');
			}
		}
		// skip control or damaged bytes
		if (ch < ' ')
			continue;
		cmd[cursor++] = (uint8_t)ch;
		cursor &= CMD_LEN;
		// clean up in case of overflow (command too long)
		if (!cursor) {
			for(uint8_t i = 0; i <= CMD_LEN; i++)
				cmd[i] = '\0';
		}
    }
}

inline bool str_is(const char *cmd, const char *str)
{
	return strcmp((const char *)cmd, str) == 0;
}

// list of supported commands 
const char *cmd_list[] = {
	"reset",
	"status",
	"poll",
	"debug rht|adc|rds|off",
	"adc chan",
	"rdsid id",
	"rdstext text",
	"freq nnnn",
	"txpwr 0-3",
	"volume 0-6",
	"mute on|off",
	"stereo on|off",
	"radio on|off",
	"gain low|off",
	"calibrate"
};

int8_t process(char *buf, rht03_t *rht)
{
	char *arg;
	char cmd[CMD_LEN + 1];

	memcpy(cmd, buf, sizeof(cmd));

	for(arg = cmd; *arg && *arg != ' '; arg++);
	if (*arg == ' ') {
		*arg = '\0';
		arg++;
	}

	if (str_is(cmd, "help")) {
		for(uint8_t i = 0; i < sizeof(cmd_list)/sizeof(cmd_list[0]); i++)
			printf("\t%s\n", cmd_list[i]);
		return 0;
	}

	if (str_is(cmd, "reset")) {
		puts_P(PSTR("\nresetting..."));
		wdt_enable(WDTO_15MS);
		while(1);
	}
	if (str_is(cmd, "calibrate")) {
		puts_P(PSTR("\ncalibrating..."));
		if (str_is(arg, "default"))
			calibrate(osccal_def);
		else
			calibrate(OSCCAL);
		return 0;
	}

	if (str_is(cmd, "poll")) {
		puts_P(PSTR("polling..."));
		rht_read(rht, RHT03_ECHO);
		ns741_rds_set_radiotext(rds_data);
		return 0;
	}

	if (str_is(cmd, "debug")) {
		if (str_is(arg, "rht")) {
			debug_flags ^= RHT03_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(debug_flags & RHT03_ECHO));
			return 0;
		}
#if ADC_MASK
		if (str_is(arg, "adc")) {
			debug_flags ^= ADC_ECHO;
			printf_P(PSTR("%s echo %s\n"), arg, is_on(rt_flags & ADC_ECHO));
			return 0;
		}
#endif
		if (str_is(arg, "off")) {
			debug_flags = 0;
			printf_P(PSTR("echo OFF\n"));
			return 0;
		}
		if (str_is(arg, "rds")) {
			ns741_rds_debug(1);
			return 0;
		}
		return -1;
	}

#if ADC_MASK
	if (str_is(cmd, "adc")) {
		// put names for your analogue inputs here
		const char *name[4] = { "Vcc", "GND", "Light", "R"};
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
		printf("ADC %d %4d %d.%dV (%s)\n", ai, val, (int)v, (int)dec, name[ai-4]);
		// if you want to use floats then do not forget to uncomment in Makefile 
		// #PRINTF_LIB = $(PRINTF_LIB_FLOAT)
		// to enable float point printing
		//double v = val*3.3/1024.0;
		//printf("ADC %d %4d %.2fV (%s)\n", ai, val, v, name[ai-4]);
		return 0;
	}
#endif

	if (str_is(cmd, "rdsid")) {
		if (*arg != '\0') {
			memset(ns741_name, 0, 8);
			for(int8_t i = 0; (i < 8) && arg[i]; i++)
				ns741_name[i] = arg[i];
			ns741_rds_set_progname(ns741_name);
			eeprom_update_block((const void *)ns741_name, (void *)em_ns741_name, 8);
		}
		printf_P(PSTR("%s %s\n"), cmd, ns741_name);
		return 0;
	}

	if (str_is(cmd, "rdstext")) {
		if (*arg == '\0') {
			puts(rds_data);
			return 0;
		}
		rt_flags |= RDS_RESET;
		if (str_is(arg, "reset")) 
			rt_flags &= ~RDS_RT_SET;
		else {
			rt_flags |= RDS_RT_SET;
			ns741_rds_set_radiotext(arg);
		}
		return 0;
	}

	if (str_is(cmd, "status")) {
		printf_P(PSTR("RDSID %s, FM %u.%02uMHz\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
			ns741_name,	ns741_freq/100, ns741_freq%100,  
			is_on(rt_flags & NS741_RADIO), is_on(rt_flags & NS741_STEREO),
			rt_flags & NS741_TXPWR, (rt_flags & NS741_VOLUME) >> 8, (rt_flags & NS741_GAIN) ? -9 : 0);
		return 0;
	}

	if (str_is(cmd, "freq")) {
		uint16_t freq = atoi((const char *)arg);
		if (freq < NS741_MIN_FREQ || freq > NS741_MAX_FREQ) {
			puts_P(PSTR("Frequency is out of band\n"));
			return -1;
		}
		freq = NS741_FREQ_STEP*(freq / NS741_FREQ_STEP);
		if (freq != ns741_freq) {
			ns741_freq = freq;
			ns741_set_frequency(ns741_freq);
			eeprom_update_word(&em_ns741_freq, ns741_freq);
		}
		printf_P(PSTR("%s set to %u\n"), cmd, ns741_freq);
		return 0;
	}

	if (str_is(cmd, "txpwr")) {
		uint8_t pwr = atoi((const char *)arg);
		if (pwr > 3) {
			puts_P(PSTR("Invalid TX power level\n"));
			return -1;
		}
		rt_flags &= ~NS741_TXPWR;
		rt_flags |= pwr;
		ns741_txpwr(pwr);
		printf_P(PSTR("%s set to %d\n"), cmd, pwr);
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, "volume")) {
		uint8_t gain = (rt_flags & NS741_VOLUME) >> 8;

		if (*arg != '\0')
			gain = atoi((const char *)arg);
		if (gain > 6) {
			puts_P(PSTR("Invalid Audio Gain value 0-6\n"));
			return -1;
		}
		ns741_volume(gain);
		printf_P(PSTR("%s set to %d\n"), cmd, gain);
		rt_flags &= ~NS741_VOLUME;
		rt_flags |= gain << 8;
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, "gain")) {
		int8_t gain = (rt_flags & NS741_GAIN) ? -9 : 0;

		if (str_is(arg, "low"))
			gain = -9;
		else if (str_is(arg, "off"))
			gain = 0;

		ns741_input_low(gain);
		printf_P(PSTR("%s is %ddB\n"), cmd, gain);
		rt_flags &= ~NS741_GAIN;
		if (gain)
			rt_flags |= NS741_GAIN;
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, "mute")) {
		if (str_is(arg, "on"))
			rt_flags |= NS741_MUTE;
		else if (str_is(arg, "off"))
			rt_flags &= ~NS741_MUTE;
		ns741_mute(rt_flags & NS741_MUTE);
		printf_P(PSTR("mute %s\n"), is_on(rt_flags & NS741_MUTE));
		return 0;
	}

	if (str_is(cmd, "stereo")) {
		if (str_is(arg, "on"))
			rt_flags |= NS741_STEREO;
		else if (str_is(arg, "off"))
			rt_flags &= ~NS741_STEREO;
		ns741_stereo(rt_flags & NS741_STEREO);
		printf_P(PSTR("stereo %s\n"), is_on(rt_flags & NS741_STEREO));
		eeprom_update_word(&em_ns741_flags, rt_flags);
		return 0;
	}

	if (str_is(cmd, "radio")) {
		if (str_is(arg, "on"))
			rt_flags |= NS741_RADIO;
		else if (str_is(arg, "off"))
			rt_flags &= ~NS741_RADIO;
		ns741_radio(rt_flags & NS741_RADIO);
		printf_P(PSTR("radio %s\n"), is_on(rt_flags & NS741_RADIO));
		return 0;
	}

	printf_P(PSTR("Unknown command '%s'\n"), cmd);
	return -1;
}

unsigned int get_char(void)
{
	static uint8_t esc = 0;
	unsigned int ch;

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
		esc = 1;
		return 0;
	}
	if (esc == 1) {
		if (ch == '[') {
			esc = 2;
			return 0;
		}
	}
	if (esc == 2) {
		esc = 0;
		if (ch == 'A')
			ch = ARROW_UP;
		if (ch == 'B')
			ch = ARROW_DOWN;
		if (ch == 'C')
			ch = ARROW_RIGHT;
		if (ch == 'D')
			ch = ARROW_LEFT;
		return ch;
	}

	esc = 0;
	/* echo */
	uart_putc((uint8_t)ch);
	return ch;
}
