/* Base Station for shDAN - Data Acquisition Network

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

#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "i2cmaster.h"

#include "spi.h"
#include "rht.h"
#include "dnode.h"
#include "mmrio.h"
#include "ns741.h"
#include "timer.h"
#include "bmp180.h"
#include "bmfont.h"
#include "serial.h"
#include "i2cmem.h"
#include "ili9225.h"
#include "rfm12bs.h"
#include "pcf2127.h"
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
uint8_t EEMEM em_ns_rt_flags = (NS741_STEREO | NS741_RDS | NS741_GAIN);
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

rht_t rht;
bmp180_t press;

rfm12_t rfm868 = {
	.mode = RFM_SPI_MODE_HW,
	.cs  = SPI_SS,
	.sck = SPI_SCK,
	.sdi = SPI_SDI,
	.sdo = SPI_SDO,
	.irq = PND3
};

ili9225_t ili = {
	.flags = ILI_LED_PIN | ILI_LED_PWM,
	.cs  = PNC3,
	.rs  = PND4,
	.rst = PNC4,
	.led = PNB3
};

// the latest message from a data node and corresponding information
dnode_t  rd;
uint16_t rd_bv;     // battery voltage
uint8_t  rd_ts[3];  // last session time
uint8_t  rd_arssi;  // last session arssi
uint8_t  rd_signal; // last session signal

dnode_status_t dans[MAX_DNODE_NUM];
uint8_t EEMEM  em_dlog[MAX_DNODE_LOGS]; // nodes for data logging
uint8_t EEMEM  em_dvalid[MAX_DNODE_NUM]; // valid nodes in the network
uint8_t EEMEM  em_dan_name[MAX_DNODE_NUM][NODE_NAME_LEN]; // Nodes' names
// track number of resets
uint16_t nreset;
uint16_t EEMEM em_nreset = 0;

static uint16_t last_ts[MAX_DNODE_LOGS];

// adc channel ARSSI connected to (> 0)
#define ARSSI_ADC 5
// ARSSI limits
#define ARSSI_IDLE (110 >> 2)
#define ARSSI_MAX  (340 >> 2)

static const char *s_pwr[4] = {
	"0.5", "0.8", "1.0", "2.0"
};

// avr-gcc does not do string pooling for PROGMEM
const char pstr_tformat[] PROGMEM = "%02d:%02d:%02d";

void update_screen(void);
void update_line(uint8_t line, uint8_t idx);

void update_radio_status(void)
{
	sprintf_P(status, PSTR("TxPwr %smW %s"), s_pwr[ns_pwr_flags & NS741_TXPWR],
		ns_pwr_flags & NS741_POWER ? "on" : "off");
	uint8_t font = bmfont_select(BMFONT_6x8);
	putlx(26, TEXT_CENTRE, status, 0);
	bmfont_select(font);
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
	uint8_t poll_clock = 3;
	nreset = eeprom_read_word(&em_nreset);
	nreset += 1;
	eeprom_write_word(&em_nreset, nreset);

	mmr_led_on(); // turn on LED while booting
	memset(dans, 0, sizeof(dans));

	for(uint8_t i = 0; i < MAX_DNODE_NUM; i++) {
		eeprom_read_block((void *)dans[i].name, (const void *)em_dan_name[i], NODE_NAME_LEN);
		if (dans[i].name[0] == '\0')
			sprintf((char *)dans[i].name, "DAN%02u", i+1);
		dans[i].name[NODE_NAME_LEN - 1] = '\0';
		dans[i].flags = eeprom_read_byte(&em_dvalid[i]);
	}

	for(uint8_t i = 0; i < MAX_DNODE_LOGS; i++) {
		uint8_t nid = eeprom_read_byte(&em_dlog[i]);
		if (nid && nid <= MAX_DNODE_NUM) {
			dans[nid-1].log = i;
			dans[nid-1].flags |= DANF_LOG;
		}
	}

	rht.valid = 0;
	rht.errors = 0;

	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_MILLIS | CLOCK_PWM);
#if _DEBUG
	{
		uint16_t ms = mill16();
		delay(1000);
		ms = mill16() - ms;
		printf_P(PSTR("delay(1000) = %u\n"), ms);
	}
#endif

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

	i2c_init(); // needed for ns741*, bmp180* and pcf2127*
	// accessing i2c memory can be quite slow, so register our
	// io handler for any pending I/O requests
	i2cmem_set_idle_callback(io_handler);

	analogReference(VREF_AVCC); // enable ADC with Vcc reference
	analogRead(ARSSI_ADC); // dummy read to start ADC
	uint8_t sync = eeprom_read_byte(&em_rfm_sync);

	spi_init(SPI_CLOCK_DIV4); // RFM12 supports only < 2.5MHz

	rfm868.mode = RFM_SPI_MODE_HW;
	rfm12_init(&rfm868, sync, RFM12_BAND_868, 868.0, RFM12_BPS_9600);

	rht_init();
	bmp180_init(&press);
	hpa[0] = '\0';

	ili9225_init(&ili);
	ili9225_set_dir(&ili, ILI9225_DISP_UPDOWN);
	ili9225_set_backlight(&ili, 127);

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
	ns_pwr_flags |= NS741_GAIN;
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

	rht_read(&rht, rt_flags & RT_ECHO_RHT, rds_data);
	mmr_rdsint_mode(INPUT_HIGHZ);
	bmfont_select(BMFONT_8x16);
	update_radio_status();
	putlx(0, 4, rds_name, 0);
	putlx(2, TEXT_CENTRE, fm_freq, TEXT_OVERLINE | TEXT_UNDERLINE);

	// turn on RDS
	ns741_rds(1);
	ns_rt_flags |= NS741_RDS;

	// reset our soft clock
	uptime   = 0;
	sw_clock = 0;
	
	rd.nid = 0;
	// initialize ADC interrupt data
	aidx = 0;
	for(uint8_t i = 0; i < 8; i++)
		areads[i] = 0;
	rd_arssi = 0;

	ADMUX  |= _BV(ADLAR); // 8 bit resolution
	ADCSRA |= _BV(ADIE);  // enable ADC interrupts

	rfm868.ridx = 0;
	rfm12_set_mode(&rfm868, RFM_MODE_RX);

	mmr_led_off();
	print_status(1);
	cli_init();

	for (uint8_t i = 0; i < MAX_DNODE_LOGS; i++)
		last_ts[i] = rd_ts[0] * 60 + rd_ts[1];

	if (pcf2127_get_time((pcf_td_t *)rd_ts, 0) == 0) {
		// RTC attached, check if time is valid
		// if backup battery is low, time can be garbage, reset
		if ((rd_ts[0] > 23) || (rd_ts[1] > 59) || (rd_ts[2] > 59)) {
			rd_ts[0] = 0;
			rd_ts[1] = 0;
			rd_ts[2] = 0;
			pcf2127_set_time((pcf_td_t *)rd_ts);
		}
		// synchronize main loop to the start of a second
		uint8_t sec = rd_ts[2];
		while(pcf2127_get_time((pcf_td_t *)rd_ts, 0) == 0) {
			if (sec != rd_ts[2])
				break;
		}
	}

	// main loop
	for(;;) {
		if (io_handler())    // keep local sensors read shifted 500 msec
			tenth_clock = 5; // to avoid collisions with the radio

		// process serial port commands
		cli_interact(cli_base, &rht);

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
			sw_clock++; 
			poll_clock ++;

			update_screen();

			uint8_t ts[3];
			if (pcf2127_get_time((pcf_td_t *)ts, 0) == 0) {
				sprintf_P(fm_freq, pstr_tformat, ts[0], ts[1], ts[2]);
				putlx(0, ILI9225_LCD_WIDTH-8*8-4, fm_freq, 0);
			}

			if ((bmp180_poll(&press, 0) == 0) && (press.valid & BMP180_P_VALID)) {
				sprintf_P(hpa, PSTR("P %u.%02u hPa"), press.p, press.pdec);
				uint8_t font = bmfont_select(BMFONT_6x8);
				putlx(4, TEXT_CENTRE, hpa, 0);
				bmfont_select(font);
			}
		}

		// poll RHT every 5 seconds
		if (poll_clock >= 5) {
			poll_clock = 0;
			putlx(2, 0, "*", 0);
			rht_read(&rht, rt_flags & RT_ECHO_RHT, rds_data);
			ns741_rds_set_radiotext(rds_data);
			putlx(2, TEXT_CENTRE, rds_data, TEXT_OVERLINE | TEXT_UNDERLINE);
			if (rt_flags & RT_ECHO_LOG) {
				uint8_t ts[3];
				pcf2127_get_time((pcf_td_t *)ts, sw_clock);
				printf_P(pstr_tformat, ts[0], ts[1], ts[2]);
				int8_t val = get_u8val(rht.temperature.val);
				printf_P(PSTR(" %d.%02d %d.%02d %d.%02d\n"),
					val, rht.temperature.dec,
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

	printf_P(pstr_tformat, rd_ts[0], rd_ts[1], rd_ts[2]);
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
		int8_t val = get_dval(rd.data.val);
		printf_P(PSTR("S%u L%u A%u E%u V %u T%+3d.%02d ARSSI %u %3d%%"),
			!!(rd.stat & STAT_SLEEP), !!(rd.stat & STAT_LED),
			!!(rd.stat & STAT_ACK), !!(rd.stat & STAT_EOS),
			rd_bv, val, rd.data.dec,
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
		printf_P(pstr_tformat, ts[0], ts[1], ts[2]);
		uart_puts("\n");
		return 0;
	}
	return CLI_ENODEV;
}

void print_node(uint8_t nid)
{
	uint8_t flags = dans[nid].flags;
	if (dans[nid].tout) {
		uint16_t vbat = dans[nid].vbat + 230;
		printf_P(pstr_tformat, dans[nid].ts[0], dans[nid].ts[1], dans[nid].ts[2]);
		printf_P(PSTR(" NID %u %5s Vbat %u ARSSI %3d%%"), nid + 1, dans[nid].name, vbat, dans[nid].ssi);
		uart_puts_p(PSTR(" Log "));
		if (flags & DANF_LOG)
			printf("  %u", dans[nid].log);
		else
			uart_puts(is_on(0));
//		uart_puts(is_on(flags & STAT_LOG));
		uart_puts_p(PSTR(" Tsync "));
		uart_puts(is_on(flags & DANF_TSYNC));
		uart_puts_p(PSTR(" Slist "));
		uart_puts(is_on(flags & DANF_SLIST));
		uart_puts_p(PSTR(" Sleep "));
		uart_puts(is_on(flags & STAT_SLEEP));
		uart_puts_p(PSTR(" Led "));
		uart_puts(is_on(flags & STAT_LED));
	}
	else {
		uart_puts_p(PSTR("not active"));
	}
	uart_puts("\n");
}

void print_status(uint8_t verbose)
{
	if (verbose) {
		uart_puts_p(PSTR("RTC time: "));
		print_rtc_time();
		if (uptime) {
			printf_P(PSTR("Resets %u, Timeouts %u, Uptime %lu sec or "),
				nreset - 1, rfm868.nto, uptime);
			if (uptime > 86400)
				printf_P(PSTR("%lu days "), uptime / 86400l);
			uint32_t utime = uptime % 86400l;
			printf_P(PSTR("%02ld:%02ld:%02ld\n"),
				utime / 3600, (utime / 60) % 60, utime % 60);
		}
	}
	get_fm_freq(fm_freq);
	printf_P(PSTR("RDSID '%s', %s\nRadio %s, Stereo %s, TX Power %d, Volume %d, Audio Gain %ddB\n"),
		rds_name, fm_freq,
		is_on(ns_pwr_flags & NS741_POWER), is_on(ns_rt_flags & NS741_STEREO),
		ns_pwr_flags & NS741_TXPWR, (ns_pwr_flags & NS741_VOLUME) >> 8, (ns_pwr_flags & NS741_GAIN) ? -9 : 0);
	if (verbose && uptime) {
		printf_P(PSTR("%s %s\n"), rds_data, hpa);
		uart_puts_p(PSTR("Active nodes:"));
		uint8_t ndan = 0;
		for(uint8_t n = 0; n < MAX_DNODE_NUM; n++) {
			if (dans[n].tout) {
				if (!ndan)
					uart_puts("\n");
				print_node(n);
				ndan++;
			}
		}
		if (!ndan)
			uart_puts_p(PSTR(" none\n"));
		if (rd.nid) {
			uart_puts_p(PSTR("Last session:\n"));
			print_rd();
		}
	}
}

void putlx(uint8_t line, uint8_t x, const char *str, uint8_t atr)
{
	line *= 8;
	uint8_t pos = x;
	uint8_t gh = 0;
	uint16_t len = 0;
//	spi_set_clock(SPI_CLOCK_DIV2);
	if (x == TEXT_CENTRE) {
		bmfont_t *font = bmfont_get();
		uint8_t gw = font->gw;
		gh = font->gh;
		for(pos = 0; str[pos]; len+=gw, pos++);
		if (len > ILI9225_LCD_WIDTH)
			pos = 0;
		else
			pos = (ILI9225_LCD_WIDTH - len) / 2;
		ili9225_swap_color(&ili);
		ili9225_fill(&ili, 0, line, pos-1, line + gh);
		ili9225_fill(&ili, pos + len, line, ILI9225_LCD_WIDTH, line + gh);
		ili9225_swap_color(&ili);
	}

	ili9225_text(&ili, pos, line, str, atr);
//	spi_set_clock(SPI_CLOCK_DIV4);
}

uint8_t io_handler(void)
{
	uint8_t ret;
	// set watchdog timer to 20 sec just in case if radio fails
	rtc_set_wdt(20);
	// RDSPIN is low when NS741 is ready to transmit next RDS frame
	if (mmr_rdsint_get() == LOW)
		ns741_rds_isr();
	
	// RFM sessions processing
	// Enable ARSSI signal reading
	uint8_t rx_flags = ARSSI_ADC;
	if (rt_flags & RT_ECHO_RX)
		rx_flags |= RFM_RX_DEBUG;

	ret = rfm12_receive_data(&rfm868, &rd, sizeof(rd), rx_flags);

	if (ret == sizeof(rd)) {
		uint8_t dan = GET_NID(rd.nid);
		if (!dan || dan > NODE_LBS)
			goto restart_rx;

		pcf2127_get_time((pcf_td_t *)rd_ts, sw_clock);
		if (dan != NODE_LBS) {
			dan -= 1;
			dans[dan].flags &= DANF_MASK;
			dans[dan].flags |= DANF_ACTIVE;
			dans[dan].nid   = dan;
			dans[dan].ts[0] = rd_ts[0];
			dans[dan].ts[1] = rd_ts[1];
			dans[dan].ts[2] = rd_ts[2];
			dans[dan].tout = 5; // reset timeout to 5 minutes

			if (!(dans[dan].flags & DANF_VALID))
				goto restart_rx;
		}

		if (rd.nid & NODE_TSYNC) { // remote node requests time sync
			dnode_t tsync;
			tsync.raw[0] = rd_ts[0];
			tsync.raw[1] = rd_ts[1];
			tsync.raw[2] = rd_ts[2];
			tsync.nid = NODE_TSYNC;
			ts_pack(&tsync, dan);
			if (dan != NODE_LBS)
				dans[dan].flags |= DANF_TSYNC;
			rfm12_send(&rfm868, &tsync, sizeof(tsync));
			if (rt_flags & RT_ECHO_DAN) {
				printf_P(pstr_tformat, rd_ts[0], rd_ts[1], rd_ts[2]);
				printf_P(PSTR(" sync %02X\n"), GET_NID(rd.nid));
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
		if (dan == NODE_LBS)
			goto restart_rx;

		dans[dan].ssi = rd_signal;

		if ((rd.nid & SENS_MASK) == SENS_LIST) {
			dans[dan].flags |= DANF_SLIST;
		}
		else {
			dans[dan].sdata[0].v16 = rd.data.v16;

			rd_bv = (rd.stat & STAT_VBAT) * 10;
			dans[dan].flags |= rd.stat & ~DANF_MASK;
			dans[dan].vbat = rd_bv;
			rd_bv += 230;

			if (dans[dan].flags & DANF_LOG) {
				uint16_t ridx = rd_ts[0]*60 + rd_ts[1];
				uint16_t i = ridx;

				if (ridx != last_ts[dans[dan].log])
					i = log_next_rec_index(last_ts[dans[dan].log]);
				for(; i != ridx; i = log_next_rec_index(i))
					log_erase_rec(dans[dan].log, i);

				dnode_log_t rec;
				rec.ssi = rd_signal | 0x80;
				rec.data.val = rd.data.val;
				rec.data.dec = rd.data.dec;
				last_ts[dans[dan].log] = ridx;
				log_write_rec(dans[dan].log, ridx, &rec);
			}

			if (rt_flags & RT_ECHO_DAN)
				print_rd();
		}
restart_rx:
		rfm12_set_mode(&rfm868, RFM_MODE_RX);
	}
	// disable watchdog timer
	rtc_set_wdt(0);
	return ret;
}

void update_line(uint8_t line, uint8_t idx)
{
	dnode_status_t *dan = &dans[idx];

	if (dan->tout < 4) {
		if (dan->tout < 2)
			ili9225_set_bk_color(&ili, RGB16_RED);
		else {
			ili9225_set_fg_color(&ili, RGB16_BLACK);
			ili9225_set_bk_color(&ili, RGB16_YELLOW);
		}
	}
	int8_t val = get_dval(dan->sdata[0].val);
	sprintf_P(fm_freq, PSTR("%-5s T% 3d.%02d "), dan->name, val, dan->sdata[0].dec);
	putlx(line, 4, fm_freq, 0);
	ili9225_set_fg_color(&ili, RGB16_WHITE);
	ili9225_set_bk_color(&ili, RGB16_BLACK);

	uint8_t font = bmfont_select(BMFONT_6x8);
	const uint16_t vbat = dan->vbat + 230;
	if (vbat <= 280) {
		if (vbat <= 260)
			ili9225_set_bk_color(&ili, RGB16_RED);
		else {
			ili9225_set_fg_color(&ili, RGB16_BLACK);
			ili9225_set_bk_color(&ili, RGB16_YELLOW);
		}
	}

	sprintf_P(status, PSTR("V %d.%02d"), vbat / 100, vbat % 100);
	putlx(line, ILI9225_LCD_WIDTH - 6 * 6 - 4, status, 0);
	ili9225_set_fg_color(&ili, RGB16_WHITE);
	ili9225_set_bk_color(&ili, RGB16_BLACK);

	const uint8_t rssi = dan->ssi;
	if (rssi < 50) {
		if (rssi < 30)
			ili9225_set_bk_color(&ili, RGB16_RED);
		else {
			ili9225_set_fg_color(&ili, RGB16_BLACK);
			ili9225_set_bk_color(&ili, RGB16_YELLOW);
		}
	}

	sprintf_P(status, PSTR("S %2d%% "), rssi);
	putlx(line + 1, ILI9225_LCD_WIDTH - 6 * 6 - 4, status, 0);
	ili9225_set_fg_color(&ili, RGB16_WHITE);
	ili9225_set_bk_color(&ili, RGB16_BLACK);
	bmfont_select(font);
}

static int8_t get_node_line(uint8_t nid)
{
	uint8_t idx = 0;
	for (uint8_t n = 0; n < MAX_DNODE_NUM; n++) {
		if (dans[n].tout) {
			if (dans[n].nid == nid)
				break;
			idx++;
		}
	}
	if (idx < MAX_NODES_PER_SCREEN)
		return idx * 2;
	return -1;
}

void update_screen(void)
{
	static uint8_t minute = 60;

	minute -= 1;
	for (uint8_t i = 0; i < MAX_DNODE_NUM; i++) {
		if (!minute) {
			if (dans[i].tout) {
				dans[i].tout -= 1;
				dans[i].flags |= DANF_ACTIVE;
			}
		}
		if ((dans[i].flags & (DANF_VALID | DANF_ACTIVE)) == (DANF_VALID | DANF_ACTIVE)) {
			dans[i].flags &= ~DANF_ACTIVE;
			int8_t line = get_node_line(i);
			if (line >= 0)
				update_line(line + 5, i);
		}
	}
	if (!minute)
		minute = 60;
}