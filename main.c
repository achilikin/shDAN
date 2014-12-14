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
#include "main.h"
#include "ns741.h"
#include "timer.h"
#include "serial.h"
#include "pcf2127.h"
#include "mmr70pin.h"
#include "ossd_i2c.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some default variables we want to store in EEPROM
uint8_t  EEMEM em_rds_name[8] = "MMR70mod"; // NS471 "station" name
uint16_t EEMEM em_radio_freq  = 9700; // 97.00 MHz

// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = 181;

// runtime flags
uint16_t EEMEM em_rt_flags = (RADIO_STEREO | RADIO_RDS | RADIO_TXPWR0 | LOAD_OSCCAL);

char rds_name[9]; // RDS PS name
char rds_data[61];  // RDS RT string
char fm_freq[17];   // FM frequency
char status[17];

uint16_t radio_freq;
uint16_t rt_flags;
uint8_t  debug_flags;

uint32_t uptime;
uint32_t sw_clock;

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
	sprintf_P(buf, PSTR("TxPwr %smW %s"), s_pwr[rt_flags & RADIO_TXPWR], 
		rt_flags & RADIO_POWER ? "on" : "off");
}

int main(void)
{
	uint8_t poll_clock = 0;

	rht_t rht;
	rht.valid = 0;
	rht.errors = 0;

	// initialise all components
	// read settings from EEPROM
	uint16_t ns741_word = eeprom_read_word(&em_rt_flags);
	rt_flags = ns741_word;
	debug_flags = 0;

	sei();
	uint8_t new_osccal = 0;
	if (rt_flags & LOAD_OSCCAL)
		new_osccal = eeprom_read_byte(&em_osccal);
	serial_init(new_osccal);
	rht_init();
	cli_init();
	i2c_init(); // needed for ns741_* and ossd_*
	ossd_init(OSSD_UPDOWN);
	ossd_select_font(OSSD_FONT_6x8);

	// initialize NS741 chip	
	eeprom_read_block((void *)rds_name, (const void *)em_rds_name, 8);
	ns741_rds_set_progname(rds_name);
	// initialize ns741 with default parameters
	ns741_init();
	// read radio frequency from EEPROM
	radio_freq = eeprom_read_word(&em_radio_freq);
	if (radio_freq < NS741_MIN_FREQ) radio_freq = NS741_MIN_FREQ;
	if (radio_freq > NS741_MAX_FREQ) radio_freq = NS741_MAX_FREQ;
	ns741_set_frequency(radio_freq);
	ns741_txpwr(ns741_word & RADIO_TXPWR);
	ns741_stereo(ns741_word & RADIO_STEREO);
	ns741_gain(ns741_word & RADIO_GAIN);
	ns741_volume((ns741_word & RADIO_VOLUME) >> 8);
	// turn ON
	if (rt_flags & RADIO_POWER)
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
	sprintf_P(fm_freq, PSTR("FM %u.%02uMHz"), radio_freq/100, radio_freq%100);
	printf_P(PSTR("ID %s, %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n> "),
		rds_name,	fm_freq,
		is_on(rt_flags & RADIO_POWER), is_on(rt_flags & RADIO_STEREO),
		rt_flags & RADIO_TXPWR, (rt_flags & RADIO_VOLUME) >> 8, (rt_flags & RADIO_GAIN) ? -9 : 0);

	rht_read(&rht, debug_flags & RHT_ECHO);
	mmr_rdsint_mode(INPUT_HIGHZ);

	get_tx_pwr(status);
	ossd_putlx(7, -1, status, 0);

	ossd_select_font(OSSD_FONT_8x16);
	ossd_putlx(0, -1, rds_name, 0);
	ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);

	// turn on RDS
	ns741_rds(1);
	rt_flags |= RADIO_RDS;
	rt_flags |= RDS_RESET; // set reset flag so next poll of RHT will start new text

	// reset our soft clock
	uptime   = 0;
	sw_clock = 0;

	for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();

		// process serial port commands
		cli_interact(&rht);

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
			sw_clock++; 
			poll_clock ++;

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

		// poll RHT every 5 seconds
		if (poll_clock >= 5) {
			poll_clock = 0;
			ossd_putlx(4, 0, "*", 0);
			rht_read(&rht, debug_flags & RHT_ECHO);
			ossd_putlx(4, -1, rds_data, 0);
			if (debug_flags & RHT_LOG) {
				uint8_t ts[3];
				if (pcf2127_get_time((pcf_td_t *)ts) != 0) {
					ts[0] = (sw_clock / 3600) % 24;
					ts[1] = (sw_clock / 60) % 60;
					ts[2] = sw_clock % 60;
				}
				printf_P(PSTR("\n%02d:%02d:%02d %d.%02d %d.%02d"), 
					ts[0], ts[1], ts[2],
					rht.temperature.val, rht.temperature.dec,
					rht.humidity.val, rht.humidity.dec);
			}
		}
    }
}
