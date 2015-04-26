/* Base Station for data acquisition network

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
#include "bmp180.h"
#include "serial.h"
#include "rfm12bs.h"
#include "pcf2127.h"
#include "ossd_i2c.h"
#include "serial_cli.h"

#include "base_main.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some default variables we want to store in EEPROM
uint8_t  EEMEM em_rds_name[8] = "BASE 01 "; // Base station name
uint16_t EEMEM em_radio_freq  = 9700; // 97.00 MHz

// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
uint8_t EEMEM em_osccal = 181;

// NS741 flags
uint8_t EEMEM em_ns_rt_flags = (NS741_STEREO | NS741_RDS);
uint8_t EEMEM em_ns_pwr_flags = NS741_TXPWR0;

// RFM12B sync pattern, better keep it to default 0xD4
// as previous versions of RFM12 do not support anything else
uint8_t EEMEM em_rfm_sync = 0xD4;

uint8_t EEMEM em_rt_flags = RT_LOAD_OSCCAL;

char rds_name[9];  // RDS PS name
char fm_freq[17];  // FM frequency
char rds_data[61]; // RDS RT string
char hpa[17];	   // pressure in hPa
char status[17];   // TxPwr status

uint16_t radio_freq;
uint8_t  ns_rt_flags;
uint8_t  ns_pwr_flags;
uint8_t  rt_flags;

uint32_t uptime;
uint32_t sw_clock;

bmp180_t press;

// the latest message from a data node and corresponding information
dnode_t  rd;
uint16_t rd_bv;     // battery voltage
uint8_t  rd_ts[3];  // last session time
uint8_t  rd_arssi;  // last session arssi
uint8_t  rd_signal; // last session signal

dnode_status_t dans[MAX_DNODES];

// adc channel ARSSI connected to (> 0)
#define ARSSI_ADC 5
// ARSSI limits
#define ARSSI_IDLE (110 >> 2)
#define ARSSI_MAX  (340 >> 2)

static const char *s_pwr[4] = {
	"0.5", "0.8", "1.0", "2.0"
};

// avr-gcc does not do string pooling for PROGMEM
const char pstr_time[] PROGMEM = "%02d:%02d:%02d";

void get_tx_pwr(char *buf)
{
	sprintf_P(buf, PSTR("TxPwr %smW %s"), s_pwr[ns_pwr_flags & NS741_TXPWR], 
		ns_pwr_flags & NS741_POWER ? "on" : "off");
}

void get_fm_freq(char *buf)
{
	sprintf_P(buf, PSTR("FM %u.%02uMHz"), radio_freq/100, radio_freq%100);
}

// simple bubble sort for uint8 arrays
static void bsort(uint8_t *data, int8_t len)
{
	len -= 1;
	for(int8_t i = 0; i < len; i++) {
		for(int8_t j = 0; j < (len - i); j++) {
			if (data[j] < data[j+1]) {
				register uint8_t t = data[j];
				data[j] = data[j+1];
				data[j+1] = t;
			}
		}
	}
}

static volatile uint8_t aidx;
static uint8_t  areads[8];

ISR(ADC_vect)
{
	areads[aidx] = ADCH;
	aidx = (aidx + 1) & 0x07;
}

int main(void)
{
	rht_t rht;
	uint8_t poll_clock = 0;

	mmr_led_on(); // turn on LED while booting

	memset(dans, 0, sizeof(dans));
	rht.valid = 0;
	rht.errors = 0;

	// initialise all components
	// read settings from EEPROM
	ns_rt_flags = eeprom_read_byte(&em_ns_rt_flags);
	ns_pwr_flags = eeprom_read_byte(&em_ns_pwr_flags);

	rt_flags = eeprom_read_byte(&em_rt_flags);

	sei();
	serial_init(UART_BAUD_RATE);
	if (rt_flags & RT_LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	i2c_init(); // needed for ns741*, ossd*, bmp180* and pcf2127*
	analogReference(VREF_AVCC); // enable ADC with Vcc reference
	analogRead(ARSSI_ADC); // dummy read to start ADC
	uint8_t sync = eeprom_read_byte(&em_rfm_sync);
	rfm12_init(sync, RFM12_BAND_868, 868.0, RFM12_BPS_9600);
	rfm12_set_mode(RFM_MODE_RX);

	rht_init();
	bmp180_init(&press);
	hpa[0] = '\0';
	ossd_init(OSSD_UPDOWN);

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
	
	// we use radio for RDS only, so set and to -9db and volume to minimum 
	ns741_mute(NS741_MUTE);
	// turn on -9db gain
	ns741_gain(NS741_GAIN);
	// read radio frequency from EEPROM
	radio_freq = eeprom_read_word(&em_radio_freq);
	if (radio_freq < NS741_MIN_FREQ) radio_freq = NS741_MIN_FREQ;
	if (radio_freq > NS741_MAX_FREQ) radio_freq = NS741_MAX_FREQ;
	get_fm_freq(fm_freq);
	ns741_set_frequency(radio_freq);
	ns741_txpwr(ns_pwr_flags & NS741_TXPWR);
	ns741_stereo(NS741_STEREO);
	// set volume to 0
	ns741_volume(0);
	ns741_mute(0); // unmute to transmit RDS

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
	rht_read(&rht, rt_flags & RT_ECHO_RHT, rds_data);
	mmr_rdsint_mode(INPUT_HIGHZ);

	ossd_select_font(OSSD_FONT_6x8);
	get_tx_pwr(status);
	ossd_putlx(7, -1, status, 0);
	ossd_select_font(OSSD_FONT_8x16);
	ossd_putlx(0, -1, rds_name, 0);
	ossd_putlx(2, -1, fm_freq, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);

	// turn on RDS
	ns741_rds(1);
	ns_rt_flags |= NS741_RDS;

	// reset our soft clock
	uptime   = 0;
	sw_clock = 0;
	
	mmr_led_off();
	print_status(0);
	cli_init();

	rd.nid = 0;
	// initialize ADC interrupt data
	aidx = 0;
	for(uint8_t i = 0; i < 8; i++)
		areads[i] = 0;
	rd_arssi = 0;

	ADMUX  |= _BV(ADLAR); // 8 bit resolution
	ADCSRA |= _BV(ADIE);  // enable ADC interrupts

	for(;;) {
		// RDSPIN is low when NS741 is ready to transmit next RDS frame
		if (mmr_rdsint_get() == LOW)
			ns741_rds_isr();
		uint8_t rx_flags = ARSSI_ADC;
		if (rt_flags & RT_ECHO_RX)
			rx_flags |= RFM_RX_DEBUG;

		if (rfm12_receive_data(&rd, sizeof(rd), rx_flags) == sizeof(rd)) {
			uint8_t dan = GET_NID(rd.nid);
			if (!dan || dan > MAX_DNODES)
				goto nodan;
			dan -= 1;
			analogStop(); // stop any pending ARSSI conversion
			pcf2127_get_time((pcf_td_t *)rd_ts, sw_clock);
			dans[dan].flags &= 0x0F;
			dans[dan].flags |= STAT_ACTIVE;
			dans[dan].ts[0] = rd_ts[0];
			dans[dan].ts[1] = rd_ts[1];
			dans[dan].ts[2] = rd_ts[2];

			if (rd.nid & NODE_TSYNC) { // remote node requests time sync
				dnode_t tsync;
				tsync.data[0] = rd_ts[0];
				tsync.data[1] = rd_ts[1];
				tsync.data[2] = rd_ts[2];
				tsync.nid = NODE_TSYNC;
				ts_pack(&tsync, dan);
				dans[dan].flags |= STAT_TSYNC;

				rfm12_send(&tsync, sizeof(tsync));
				rfm12_set_mode(RFM_MODE_RX);
				if (rt_flags & RT_ECHO_DAN) {
					printf_P(pstr_time, rd_ts[0], rd_ts[1], rd_ts[2]);
					printf_P(PSTR(" sync %02X\n"), rd.nid);
				}
			}
			
			// we should have at least 7 ARSSI reads in our areads array
			bsort(areads, 8);
			rd_arssi  = 0;
			rd_signal = 0;
			// use two reads from the middle and reset ARSSI array
			uint16_t average = 0;
			if (rt_flags & RT_ECHO_RX)
				uart_puts_p(PSTR("ARSSI"));
			for(uint8_t i = 0; i < 8; i++) {
				if (rt_flags & RT_ECHO_RX)
					printf_P(PSTR(" %u"), areads[i]);
				if (i == 3 || i == 4)
					average += areads[i];
				areads[i] = 0;
			}
			if (rt_flags & RT_ECHO_RX)
				uart_puts("\n");

			rd_arssi = average / 2;
			aidx = 0;
			if (rd_arssi) {
				uint8_t arssi = rd_arssi;
				if (arssi < ARSSI_IDLE)
					arssi = ARSSI_IDLE;
				rd_signal = ((100*(arssi - ARSSI_IDLE))/(ARSSI_MAX - ARSSI_IDLE));
				if (rd_signal > 100)
					rd_signal = 100;
			}
			dans[dan].ssi = rd_signal;

			if (rt_flags & RT_ECHO_DAN)
				print_rd();

			if ((rd.nid & SENS_MASK) == SENS_LIST) {
				dans[dan].flags |= STAT_SLIST;
			}
			else {
				rd_bv = (rd.stat & STAT_VBAT)* 10;
				dans[dan].flags |= rd.stat & ~STAT_VBAT;
				dans[dan].vbat = rd_bv;
				rd_bv += 230;

				// overwrite FM frequency on the screen
				sprintf_P(fm_freq, pstr_time, rd_ts[0], rd_ts[1], rd_ts[2]);
				ossd_putlx(0, -1, fm_freq, 0);
				sprintf_P(fm_freq, PSTR("s%02X T %d.%02d "), rd.nid, rd.val, rd.dec);
				ossd_putlx(4, -1, fm_freq, 0);
				// overwrite NS741 status
				sprintf_P(status, PSTR("V %d.%d S %d%%"), rd_bv/100, rd_bv%100, rd_signal);
				uint8_t font = ossd_select_font(OSSD_FONT_6x8);
				ossd_putlx(7, -1, status, 0);
				ossd_select_font(font);
			}
		}
nodan:
		// process serial port commands
		cli_interact(cli_base, &rht);

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
			sw_clock++; 
			poll_clock ++;

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
			ossd_putlx(2, 0, "*", 0);
			rht_read(&rht, rt_flags & RT_ECHO_RHT, rds_data);
			ossd_putlx(2, -1, rds_data, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
			if (rt_flags & RT_ECHO_LOG) {
				uint8_t ts[3];
				pcf2127_get_time((pcf_td_t *)ts, sw_clock);
				printf_P(pstr_time, ts[0], ts[1], ts[2]);
				printf_P(PSTR(" %d.%02d %d.%02d %d.%02d\n"),
					rht.temperature.val, rht.temperature.dec,
					rht.humidity.val, rht.humidity.dec,
					press.p, press.pdec);
			}
		}
    }
}

void print_rd(void)
{
	if (rd.nid == 0)
		return;

	printf_P(pstr_time, rd_ts[0], rd_ts[1], rd_ts[2]);
	uart_puts_p(PSTR(" | "));
	for(uint8_t i = 0; i < 4; i++) {
		uint8_t *pu8 = (uint8_t *)&rd;
		printf_P(PSTR("%02X "), pu8[i]);
	}
	printf_P(PSTR("| NID %u SID %u "), rd.nid & NID_MASK, (rd.nid & SENS_MASK) >> 4);

	if ((rd.nid & SENS_MASK) == SENS_LIST) {
		for(uint8_t i = 1; i <= MAX_SENSORS; i++)
			printf_P(PSTR("%02u "), get_sens_type(&rd, i));
	}
	else {
		printf_P(PSTR("S%u L%u A%u E%u V %u T%+2d.%02d ARSSI %u %3d%%"),
			!!(rd.stat & STAT_SLEEP), !!(rd.stat & STAT_LED),
			!!(rd.stat & STAT_ACK), !!(rd.stat & STAT_EOS),
			rd_bv, rd.val, rd.dec,
			rd_arssi, rd_signal);
	}
	uart_puts("\n");
}

int8_t print_rtc_time(void)
{
	uint8_t ts[3];
	if (pcf2127_get_time((pcf_td_t *)ts, 0) == 0) {
		// reset our sw clock
		sw_clock = ts[2];
		sw_clock += ts[1] * 60;
		sw_clock += ts[0] * 3600;
		printf_P(pstr_time, ts[0], ts[1], ts[2]);
		uart_puts("\n");
		return 0;
	}
	return CLI_ENODEV;
}

void print_status(uint8_t verbose)
{
	if (verbose) {
		uart_puts_p(PSTR("RTC time: "));
		print_rtc_time();
		printf_P(PSTR("Uptime %lu sec or "), uptime);
		if (uptime > 86400)
			printf_P(PSTR("%lu days "), uptime/86400l);
		uint32_t utime = uptime % 86400l;
		printf_P(PSTR("%02ld:%02ld:%02ld\n"),
			utime / 3600, (utime / 60) % 60, utime % 60);
	}
	get_fm_freq(fm_freq);
	printf_P(PSTR("RDSID '%s', %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
		rds_name, fm_freq,
		is_on(ns_pwr_flags & NS741_POWER), is_on(ns_rt_flags & NS741_STEREO),
		ns_pwr_flags & NS741_TXPWR, (ns_pwr_flags & NS741_VOLUME) >> 8, (ns_pwr_flags & NS741_GAIN) ? -9 : 0);
	if (verbose) {
		printf_P(PSTR("%s %s\n"), rds_data, hpa);
		uart_puts_p(PSTR("Active nodes:"));
		uint8_t ndan = 0;
		for(uint8_t n = 0; n < MAX_DNODES; n++) {
			uint8_t flags = dans[n].flags;
			if (flags) {
				ndan++;
				uint16_t vbat = dans[n].vbat + 230;
				uart_puts("\n");
				printf_P(pstr_time, dans[n].ts[0], dans[n].ts[1], dans[n].ts[2]);
				printf_P(PSTR(" NID %u Vbat %u ARSSI %3d%%"), n + 1, vbat, dans[n].ssi);
				uart_puts_p(PSTR(" Tsync "));
				uart_puts(is_on(flags & STAT_TSYNC));
				uart_puts_p(PSTR(" Slist "));
				uart_puts(is_on(flags & STAT_SLIST));
				uart_puts_p(PSTR(" Sleep "));
				uart_puts(is_on(flags & STAT_SLEEP));
				uart_puts_p(PSTR(" Led "));
				uart_puts(is_on(flags & STAT_LED));
			}
		}
		if (!ndan)
			uart_puts_p(PSTR(" none"));
		uart_puts("\n");
		uart_puts_p(PSTR("Last session:\n"));
		print_rd();
	}
}

