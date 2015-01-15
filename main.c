/* Example of using ATmega32 on MMR-70

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

#include "cli.h"
#include "rht.h"
#include "main.h"
#include "dnode.h"
#include "pinio.h"
#include "ns741.h"
#include "timer.h"
#include "bmp180.h"
#include "serial.h"
#include "rfm12bs.h"
#include "pcf2127.h"
#include "ossd_i2c.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

#define UART_BAUD_RATE 38400LL // 38400 at 8MHz gives only 0.2% errors

// some default variables we want to store in EEPROM
uint8_t  EEMEM em_rds_name[8] = "MMR70mod"; // NS471 "station" name
uint16_t EEMEM em_radio_freq  = 9700; // 97.00 MHz

// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = 181;

// runtime flags
uint16_t EEMEM em_rt_flags = (RADIO_STEREO | RADIO_RDS | RADIO_TXPWR0 | LOAD_OSCCAL);

char rds_name[9];  // RDS PS name
char fm_freq[17];  // FM frequency
char rds_data[61]; // RDS RT string
char hpa[17];	   // pressure in hPa
char status[17];   // TxPwr status

uint16_t radio_freq;
uint16_t rt_flags;
uint8_t  debug_flags;

uint32_t uptime;
uint32_t sw_clock;

bmp180_t press;

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

// Select mode - receiver or transmitter
#define RFM_MODE RFM_MODE_RX
// adc channel ARSSI connected to
#define ARSSI_ADC 5

dnode_t  rd;
uint16_t rd_bv;     // battery voltage
uint8_t  rd_ts[3];  // last session time
uint8_t  rd_signal; // session signal
uint16_t rd_arssi;

int main(void)
{
	rht_t rht;
	uint8_t poll_clock = 0;

	mmr_led_on(); // turn on LED while booting

	rht.valid = 0;
	rht.errors = 0;

	// initialise all components
	// read settings from EEPROM
	uint16_t ns741_word = eeprom_read_word(&em_rt_flags);
	rt_flags = ns741_word;
	debug_flags = 0;

	sei();
	serial_init(UART_BAUD_RATE);
	if (rt_flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	i2c_init(); // needed for ns741*, ossd*, bmp180* and pcf2127*

	rd_arssi = ARSSI_ADC;
	analogRead(ARSSI_ADC); // dummy read to start ADC
	rfm12_init(RFM12_BAND_868, 868.0, RFM12_BPS_9600);
#if (RFM_MODE == RFM_MODE_RX)
	rfm12_set_mode(RFM_MODE_RX);
#endif

	rht_init();
	bmp180_init(&press);
	hpa[0] = '\0';
	ossd_init(OSSD_UPDOWN);
	ossd_select_font(OSSD_FONT_6x8);

	// initialize NS741 chip	
	eeprom_read_block((void *)rds_name, (const void *)em_rds_name, 8);
	ns741_rds_set_progname(rds_name);
	// initialize ns741 with default parameters
	ns741_init();
	// turn ON
	if (rt_flags & RADIO_POWER) {
		delay(700);
		ns741_radio_power(1);
		delay(150);
	}
	ns741_gain(ns741_word & RADIO_GAIN);
	// read radio frequency from EEPROM
	radio_freq = eeprom_read_word(&em_radio_freq);
	if (radio_freq < NS741_MIN_FREQ) radio_freq = NS741_MIN_FREQ;
	if (radio_freq > NS741_MAX_FREQ) radio_freq = NS741_MAX_FREQ;
	ns741_set_frequency(radio_freq);
	ns741_txpwr(ns741_word & RADIO_TXPWR);
	ns741_stereo(ns741_word & RADIO_STEREO);
	ns741_volume((ns741_word & RADIO_VOLUME) >> 8);

	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_MILLIS);
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
		rds_name, fm_freq,
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
	
	mmr_led_off();
	cli_init();

	for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();
		
#if	(RFM_MODE == RFM_MODE_RX)
		#define ARSSI_IDLE 110
		#define ARSSI_MAX  340
		if (rfm12_receive_data(&rd, sizeof(rd), &rd_arssi) == sizeof(rd)) {
			if (rd.nid & ZONE_TSYNC) {
				dnode_t tsync;
				tsync.nid = 0;
				pcf2127_get_time((pcf_td_t *)&tsync.vbat, sw_clock);

				rfm12_set_mode(RFM_MODE_TX);
				rfm12_send(&tsync, sizeof(tsync));
				rfm12_set_mode(RFM_MODE_RX);
				if (debug_flags & RD_ECHO)
					printf_P(PSTR("%02d:%02d:%02d sync %02X\n"), tsync.vbat, tsync.val, tsync.dec, rd.nid);
			}

			if (rd_arssi & 0x8000) {
				rd_arssi &= 0x0FFF;
				if (rd_arssi < ARSSI_IDLE)
					rd_arssi = ARSSI_IDLE;
				rd_signal = ((100*(rd_arssi - ARSSI_IDLE))/(ARSSI_MAX - ARSSI_IDLE));
				if (rd_signal > 100)
					rd_signal = 100;
			}

			if (rd.nid & ZONE_MASK) {
				rt_flags |= RDATA_VALID;
				pcf2127_get_time((pcf_td_t *)rd_ts, sw_clock);
				// overwrite FM frequency on the screen
				sprintf_P(fm_freq, PSTR("s%02X T %d.%d "), rd.nid, rd.val, rd.dec);
				ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
				// overwrite NS741 status
				rd_bv = 0;
				if (rd.vbat != -1)
					rd_bv = 230 + rd.vbat * 10;
				sprintf_P(status, PSTR("V %d.%d S %d%%"), rd_bv/100, rd_bv%100, rd_signal);
				uint8_t font = ossd_select_font(OSSD_FONT_6x8);
				ossd_putlx(7, -1, status, 0);
				ossd_select_font(font);
			}

			if (debug_flags & RD_ECHO)
				print_rd();

			rd_arssi = ARSSI_ADC;
		}
#endif

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
						printf_P(PSTR("adc%d %4d %d.%02d\n"), i, val, v, dec);
					}
				}
			}
#endif
			if ((bmp180_poll(&press, 0) == 0) && (press.valid & BMP180_P_VALID)) {
				sprintf_P(hpa, PSTR("P %u.%02u hPa"), press.p, press.pdec);
				uint8_t font = ossd_select_font(OSSD_FONT_6x8);
				ossd_putlx(6, -1, hpa, 0);
				ossd_select_font(font);
			}
		}

		// poll RHT every 5 seconds
		if (poll_clock >= 5) {
			poll_clock = 0;
			ossd_putlx(4, 0, "*", 0);
			rht_read(&rht, debug_flags & RHT_ECHO);
			ossd_putlx(4, -1, rds_data, 0);
			if (debug_flags & RHT_LOG) {
				uint8_t ts[3];
				pcf2127_get_time((pcf_td_t *)ts, sw_clock);
				printf_P(PSTR("%02d:%02d:%02d %d.%d %d.%d %d.%02d\n"),
					ts[0], ts[1], ts[2],
					rht.temperature.val, rht.temperature.dec,
					rht.humidity.val, rht.humidity.dec,
					press.p, press.pdec);
			}

#if (RFM_MODE == RFM_MODE_TX)
			rd.nid  = 0x11;
			rd.val  = rht.temperature.val;
			rd.dec  = rht.temperature.dec;
			rd.vbat = rfm12_battery(RFM_MODE_IDLE, 14);
			rfm12_send((uint8_t *)&rd, sizeof(rd));
#endif
		}
    }
}

void print_rd(void)
{
	if (!(rt_flags & RDATA_VALID))
		return;

	printf_P(PSTR("%02d:%02d:%02d %02X ARSSI %u %3d%% V %d T %d.%d\n"),
		rd_ts[0], rd_ts[1], rd_ts[2], rd.nid, rd_arssi, rd_signal, rd_bv, rd.val, rd.dec);
}
