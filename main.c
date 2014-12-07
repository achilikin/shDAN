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
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "i2cmaster.h"

#include "cli.h"
#include "rht.h"
#include "ns741.h"
#include "timer.h"
#include "serial.h"
#include "mmr70pin.h"
#include "ossd_i2c.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some NS741 default variables we want to store in EEPROM
uint8_t  EEMEM em_ns741_name[8] = "MMR70mod"; // NS471 "station" name
uint16_t EEMEM em_ns741_freq  = 9700; // 97.00 MHz
uint16_t EEMEM em_ns741_flags = (NS741_STEREO | NS741_RDS | NS741_TXPWR0);

char ns741_name[9]; // RDS PS name
char rds_data[61];  // RDS RT string
char fm_freq[17];   // FM frequency
char status[17];

uint16_t ns741_freq;
uint16_t rt_flags;
uint8_t  debug_flags;

inline const char *is_on(uint8_t val)
{
	if (val) return "ON";
	return "OFF";
}

static const char *s_pwr[4] = {
		"0.5", "0.8", "1.0", "2.0"
};

void get_tx_pwr(char *buf)
{
	sprintf_P(buf, PSTR("TxPwr %smW %s"), s_pwr[rt_flags & NS741_TXPWR], 
		rt_flags & NS741_RADIO ? "on" : "off");
}

int main(void)
{
	rht_t rht;
	rht.valid = 0;
	rht.errors = 0;

	// initialise all components
	// read settings from EEPROM
	uint16_t ns741_word = eeprom_read_word(&em_ns741_flags);
	rt_flags = ns741_word;
	debug_flags = 0;

	sei();
	serial_init(LOAD_OSCCAL);
	rht_init();
	cli_init();
	i2c_init(); // needed for ns741_* and ossd_*
	ossd_init(OSSD_UPDOWN);
	ossd_select_font(OSSD_FONT_6x8);

	// initialize NS741 chip	
	eeprom_read_block((void *)ns741_name, (const void *)em_ns741_name, 8);
	ns741_rds_set_progname(ns741_name);
	// initialize ns741 with default parameters
	ns741_init();
	// read radio frequency from EEPROM
	ns741_freq = eeprom_read_word(&em_ns741_freq);
	if (ns741_freq < NS741_MIN_FREQ) ns741_freq = NS741_MIN_FREQ;
	if (ns741_freq > NS741_MAX_FREQ) ns741_freq = NS741_MAX_FREQ;
	ns741_set_frequency(ns741_freq);
	ns741_txpwr(ns741_word & NS741_TXPWR);
	ns741_stereo(ns741_word & NS741_STEREO);
	ns741_gain(ns741_word & NS741_GAIN);
	ns741_volume((ns741_word & NS741_VOLUME) >> 8);
	// turn ON
	rt_flags |= NS741_RADIO;
	ns741_radio(1);
	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_millis();

#if _DEBUG
	{
		uint16_t ms = mill16();
		delay(1000);
		ms = mill16() - ms;
		printf_P(PSTR("delay(1000) = %u\n"), ms);
		// calibrate(osccal_def);
	}
#endif
	sprintf_P(fm_freq, PSTR("FM %u.%02uMHz"), ns741_freq/100, ns741_freq%100);
	printf_P(PSTR("ID %s, %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n> "),
		ns741_name,	fm_freq,
		is_on(rt_flags & NS741_RADIO), is_on(rt_flags & NS741_STEREO),
		rt_flags & NS741_TXPWR, (rt_flags & NS741_VOLUME) >> 8, (rt_flags & NS741_GAIN) ? -9 : 0);

	rht_read(&rht, debug_flags & RHT03_ECHO);
	mmr_rdsint_mode(INPUT_HIGHZ);

	get_tx_pwr(status);
	ossd_putlx(7, -1, status, 0);

	ossd_select_font(OSSD_FONT_8x16);
	ossd_putlx(0, -1, ns741_name, 0);
	ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);

	// turn on RDS
	ns741_rds(1);
	rt_flags |= NS741_RDS;
	rt_flags |= RDS_RESET; // set reset flag so next poll of RHT will start new text

    for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();

		// process serial port commands
		cli_interact(&rht);

		// poll RHT every 5 seconds (or 50 tenth_clock)
		if (tenth_clock >= 50) {
			tenth_clock = 0;
			ossd_putlx(4, 0, "*", 0);
			rht_read(&rht, debug_flags & RHT03_ECHO);
			ossd_putlx(4, -1, rds_data, 0);

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
    }
}
