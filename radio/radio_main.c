/* Firmware for MMR-70 FM radio

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
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "i2cmaster.h"

#include "rht.h"
#include "dnode.h"
#include "pinio.h"
#include "ns741.h"
#include "timer.h"
#include "serial.h"
#include "bmfont.h"
#include "ossd_i2c.h"
#include "serial_cli.h"

#include "radio_main.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some default variables we want to store in EEPROM
uint8_t  EEMEM em_rds_name[8] = "MMR70mod"; // NS471 "station" name
uint16_t EEMEM em_radio_freq  = 9700; // 97.00 MHz

// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = 181;

// NS741 flags
uint8_t EEMEM em_ns_rt_flags = (NS741_STEREO | NS741_RDS);
uint8_t EEMEM em_ns_pwr_flags = NS741_TXPWR0;

uint8_t EEMEM em_rt_flags = LOAD_OSCCAL;

char rds_name[9];  // RDS PS name
char fm_freq[17];  // FM frequency
char rds_data[61]; // RDS RT string
char status[17];   // TxPwr status

uint16_t radio_freq;
uint8_t  ns_rt_flags;
uint8_t  ns_pwr_flags;
uint8_t  rt_flags;

uint32_t uptime;
uint32_t sw_clock;

static const char *s_pwr[4] = {
	"0.5", "0.8", "1.0", "2.0"
};

void get_tx_pwr(char *buf)
{
	sprintf_P(buf, PSTR("TxPwr %smW %s"), s_pwr[ns_pwr_flags & NS741_TXPWR], 
		ns_pwr_flags & NS741_POWER ? "on" : "off");
}

// pretty accurate conversion to 3.3V without using floats 
static uint8_t get_voltage(uint16_t adc, uint8_t *decimal)
{
	uint32_t v = adc * 323LL + 500LL;
	uint32_t dec = (v % 100000LL) / 1000LL;
	v = v / 100000LL;

	*decimal = (uint8_t)dec;
	return (uint8_t)v;
}


int main(void)
{
	rht_t rht;
	uint8_t poll_clock = 0;

	mmr_led_on(); // turn on LED while booting

	rht.valid = 0;
	rht.errors = 0;

	// initialise all components
	// read settings from EEPROM
	ns_rt_flags = eeprom_read_byte(&em_ns_rt_flags);
	ns_pwr_flags = eeprom_read_byte(&em_ns_pwr_flags);

	rt_flags = eeprom_read_byte(&em_rt_flags);

	sei();
	serial_init(UART_BAUD_RATE);
	if (rt_flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	i2c_init(); // needed for ns741* and ossd*
	rht_init();
	ossd_init(OSSD_UPDOWN);
	bmfont_select(BMFONT_6x8);
#if ADC_MASK
	analogReference(VREF_AVCC); // enable ADC with Vcc reference
#endif
	// initialize NS741 chip	
	eeprom_read_block((void *)rds_name, (const void *)em_rds_name, 8);
	ns741_rds_set_progname(rds_name);
	// initialize ns741 with default parameters
	ns741_init();
	// turn ON
	if (ns_pwr_flags & NS741_POWER) {
		delay(700);
		ns741_radio_power(1);
		delay(150);
	}
	ns741_gain(ns_pwr_flags & NS741_GAIN);
	// read radio frequency from EEPROM
	radio_freq = eeprom_read_word(&em_radio_freq);
	if (radio_freq < NS741_MIN_FREQ) radio_freq = NS741_MIN_FREQ;
	if (radio_freq > NS741_MAX_FREQ) radio_freq = NS741_MAX_FREQ;
	ns741_set_frequency(radio_freq);
	ns741_txpwr(ns_pwr_flags & NS741_TXPWR);
	ns741_stereo(ns_rt_flags & NS741_STEREO);
	ns741_volume((ns_pwr_flags & NS741_VOLUME) >> 8);

	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_MILLIS);
#if _DEBUG
	{
		uint16_t ms = mill16();
		delay(1000);
		ms = mill16() - ms;
		printf_P(PSTR("delay(1000) = %u\n"), ms);
	}
#endif
	sprintf_P(fm_freq, PSTR("FM %u.%02uMHz"), radio_freq/100, radio_freq%100);
	printf_P(PSTR("ID %s, %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n> "),
		rds_name, fm_freq,
		is_on(ns_pwr_flags & NS741_POWER), is_on(ns_rt_flags & NS741_STEREO),
		ns_pwr_flags & NS741_TXPWR, (ns_pwr_flags & NS741_VOLUME) >> 8, (ns_pwr_flags & NS741_GAIN) ? -9 : 0);

	rht_read(&rht, rt_flags & RHT_ECHO, rds_data);
	mmr_rdsint_mode(INPUT_HIGHZ);

	get_tx_pwr(status);
	ossd_putlx(7, -1, status, 0);

	bmfont_select(BMFONT_8x16);
	ossd_putlx(0, -1, rds_name, 0);
	ossd_putlx(2, -1, fm_freq, TEXT_OVERLINE | TEXT_UNDERLINE);

	// turn on RDS
	ns741_rds(1);
	ns_rt_flags |= NS741_RDS;
	ns_rt_flags |= RDS_RESET; // set reset flag so next poll of RHT will start new text

	// reset our soft clock
	uptime   = 0;
	sw_clock = 0;
	
	mmr_led_off();
	cli_init();

	for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();
		// process serial port commands
		cli_interact(cli_radio, &rht);

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
			sw_clock++;
			if (sw_clock == 86400)
				sw_clock = 0;
			poll_clock++;

			if (ns_rt_flags & RDS_RESET) {
				ns741_rds_reset_radiotext();
				ns_rt_flags &= ~RDS_RESET;
			}

			if (!(ns_rt_flags & RDS_RT_SET))
				ns741_rds_set_radiotext(rds_data);
#if ADC_MASK
			if (rt_flags & ADC_ECHO) {
				// if you want to use floats enable PRINTF_LIB_FLOAT in the makefile
				puts("");
				for(uint8_t i = 0; i < 8; i++) {
					if (ADC_MASK & (1 << i)) {
						uint16_t val = analogRead(i);
						uint8_t dec;
						uint8_t v = get_voltage(val, &dec);
						printf_P(PSTR("adc%d %4d %d.%02d\n"), i, val, v, dec);
					}
				}
			}
#endif
		}

		// poll RHT every 5 seconds
		if (poll_clock >= 5) {
			poll_clock = 0;
			ossd_putlx(4, 0, "*", 0);
			rht_read(&rht, rt_flags & RHT_ECHO, rds_data);
			ossd_putlx(4, -1, rds_data, 0);
			if (rt_flags & RHT_LOG) {
				printf_P(PSTR("%02d:%02d:%02d %d.%d %d.%d\n"),
					sw_clock / 3600, (sw_clock / 60) % 60, sw_clock % 60,
					rht.temperature.val, rht.temperature.dec,
					rht.humidity.val, rht.humidity.dec);
			}
		}
    }
}
