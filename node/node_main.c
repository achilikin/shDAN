/* Data Node for data acquisition network

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
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

// Peter Fleury's UART and I2C libraries 
// http://homepage.hispeed.ch/peterfleury/avr-software.html
#include "i2cmaster.h"

#include "dnode.h"
#include "pinio.h"
#include "timer.h"
#include "bmp180.h"
#include "serial.h"
#include "rfm12bs.h"
#include "ossd_i2c.h"
#include "serial_cli.h"

#include "node_main.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// default configuration
#define DEF_NID		1   // node id
#define DEF_TXPWR	0   // by default RFM12 uses max TX power
#define DEF_OSCCAL	178 // average value from serial_calibrate()

// configuration variables stored in EEPROM
uint8_t EEMEM em_nid = DEF_NID;
uint8_t EEMEM em_txpwr = DEF_TXPWR;
uint8_t EEMEM em_osccal = DEF_OSCCAL;

uint8_t EEMEM em_flags = LOAD_OSCCAL; // runtime flags

// I/O pins
#define PIN_INTERACTIVE PB0 // on port B

#define ZONE_T1 1

#define SYNC_SPAN     20 // request time sync every 20 transmissions
#define REPLY_TIMEOUT 60 // time sync reply timeout, must be less than 127 msec

#define TIME_TO_POLL(x) (rtc_sec == (((nid - 1)*5) + (x - 1)))

uint8_t  flags;
uint8_t  active;
uint32_t uptime;
bmp180_t bmp;

dnode_t  rd;
uint8_t  nid;   // node id
uint8_t  txpwr; // RFM12 TX power

uint16_t rd_bv;     // battery voltage
uint8_t  rd_ts[3];  // last session time

#define power_twi_disable() (TWCR &= ~_BV(TWEN))
#define power_adc_disable() (ADCSRA &= ~_BV(ADEN))
#define power_timer1_disable() (TIMSK &= ~_BV(OCIE1A))
#define power_timer1_enable()  (TIMSK |= _BV(OCIE1A))
#define power_twi_enable() (TWCR |= _BV(TWEN))

#if (RFM_SPI_MODE == RFM_SPI_MODE_SW)
#define power_spi_disable()
#define power_spi_enable()
#else
#define power_spi_disable() (SPCR &= ~_BV(SPE))
#define power_spi_enable() (SPCR |= _BV(SPE))
#endif

static void show_time(char *buf);

// disable all unused interfaces
static void asleep(void)
{
	power_spi_disable();
	power_twi_disable();
	power_adc_disable();
}

static void awake(void)
{
	power_twi_enable();
	power_spi_enable();
}

static void sleep(uint8_t mode)
{
	// set sleep mode
	set_sleep_mode(mode);
	asleep();

	power_timer1_disable(); // our msec timer
	sleep_enable();
	sei();
	sleep_cpu();
	sleep_disable();
	power_timer1_enable();
}

static inline uint8_t is_interactive(void)
{
	return get_pinb(PIN_INTERACTIVE) ^ _BV(PIN_INTERACTIVE);
}

// returns -1 in case of error, or reply interval in ms (<60)
int8_t sync_time(uint8_t send)
{
	dnode_t tsync;

	if (send) {
		tsync.nid = nid | NODE_TSYNC | SENS_LIST;
		tsync.data[0] = SENS_TEMPER;
		tsync.data[1] = 0;
		tsync.data[2] = 0;
		rfm12_send(&tsync, sizeof(tsync));
	}

	rfm12_set_mode(RFM_MODE_RX);
	rfm12_reset_fifo();
	rfm12_cmdrw(RFM12CMD_STATUS); // clear any interrupts

	uint8_t msec = mill8();
	for(uint8_t dt = 0; dt < REPLY_TIMEOUT; dt = mill8() - msec) {
		if (rfm12_receive_data(&tsync, sizeof(tsync), 0) == sizeof(tsync)) {
			ATOMIC_BLOCK(ATOMIC_FORCEON) {
			rtc_hour = tsync.hour;
			rtc_min  = tsync.min;
			rtc_sec  = tsync.sec;
			}
			return dt;
		}
	}

	return -1;
}

int main(void)
{
	char buf[16];
	uint8_t tsync = 0;

	mmr_led_on(); // turn on LED while booting

	uptime = 0;
	active = 0;
	MCUCSR |= _BV(JTD); // disable JTag if enabled in the fuses

	sei();

	nid = eeprom_read_byte(&em_nid); // our node id
	txpwr = eeprom_read_byte(&em_txpwr);
	flags = eeprom_read_byte(&em_flags);

	// RFM12 nIRQ pin
	_pin_mode(&DDRD, _BV(nIRQ), INPUT_HIGHZ);
	// interactive mode pin, if grounded then MCU will not enter sleep state
	// and will read serial port for commands and display data on oled screen
	_pin_mode(&DDRB, _BV(PIN_INTERACTIVE), INPUT_UP);

	// setup RTC timer with external 32768 crystal and
	// setup our ~millisecond timer for mill*() and tenth_clock counter
	init_time_clock(CLOCK_RTC | CLOCK_MILLIS);
	mmr_led_off();

	delay(1000); // to make sure that our 32kHz crystal is up and running

	// turn LED on during initialization stage
	mmr_led_on();
	serial_init(UART_BAUD_RATE);
	if (flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	rfm12_init(0xD4, RFM12_BAND_868, 868.0, RFM12_BPS_9600);
	rfm12_set_txpwr(txpwr);
	rd.stat = rfm12_battery(RFM_MODE_SLEEP, 14);

	mmr_led_off();
	// try to get RTC time from the base
	for(uint8_t i = 0; i < 5; i++) {
		if (sync_time(1) != -1)
			break;
		delay(217);
	}
	mmr_led_on();

	i2c_init(); // needed for ossd* and bmp180*
	// try to init oled display
	if (ossd_init(OSSD_UPDOWN) == 0) {
		ossd_select_font(OSSD_FONT_8x16);
		sprintf_P(buf, PSTR("Node %d"), nid);
		ossd_putlx(0, -1, buf, 0);
		show_time(buf);
		active |= OLED_ACTIVE;
	}

	bmp180_init(&bmp);
	cli_init();
	flags |= DATA_INIT;
	mmr_led_off();

	if (is_interactive()) {
		active |= UART_ACTIVE;
		print_status();
	}
	else {
		if (active & OLED_ACTIVE)
			ossd_sleep(1);
		sleep(SLEEP_MODE_PWR_SAVE);
	}

	for(;;) {
		// process serial port commands if in interactive mode
		if (is_interactive()) {
			// bring cli and screen out of sleep condition
			if (!(active & UART_ACTIVE)) {
				awake();
				if (active & OLED_ACTIVE)
					ossd_sleep(0);
				rd.stat = rfm12_battery(RFM_MODE_IDLE, 14);
				uart_puts_p(PSTR("\n> "));
				active |= UART_ACTIVE;
			}
			cli_interact(cli_node, &rd);
		}

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
		}

		// poll bmp180 for temperature only every 1 minute
		if ((flags & (DATA_POLL | DATA_INIT)) || (TIME_TO_POLL(ZONE_T1) && !(flags & DATA_SENT))) {
			awake();
			if (active & DLED_ACTIVE)
				mmr_led_on();
			bmp180_poll(&bmp, BMP180_T_MODE);
			if (active & DLED_ACTIVE)
				mmr_led_off();

			// Local Sensor Data log
			if ((flags & (LSD_ECHO | DATA_POLL)) && (active & UART_ACTIVE))
				print_lsd();

			if (bmp.valid & BMP180_T_VALID) {
				rd.nid = SET_NID(nid,ZONE_T1);
				if (tsync == SYNC_SPAN) {
					rd.nid |= NODE_TSYNC;
					rd.stat = rfm12_battery(RFM_MODE_IDLE, 14);
				}
				rd.stat &= ~(STAT_LED | STAT_SLEEP);
				if (active & DLED_ACTIVE)
					rd.stat |= STAT_LED;
				if (!(active & UART_ACTIVE))
					rd.stat |= STAT_SLEEP;
				rd.val = bmp.t;
				rd.dec = bmp.tdec;
				rfm12_send(&rd, sizeof(rd));

				// get time sync reply
				if (tsync == SYNC_SPAN) {
					sync_time(0);
					tsync = 0;
				}

				rfm12_set_mode(RFM_MODE_SLEEP);
				flags |= DATA_SENT;
				tsync++;
			}

			if (is_interactive() && (active & OLED_ACTIVE)) {
				show_time(buf);
				get_vbat(buf);
				ossd_putlx(6, -1, buf, 0);
			}

			serial_wait_sending();
			flags &= ~(DATA_POLL | DATA_INIT);
		}

		if (!TIME_TO_POLL(ZONE_T1))
			flags &= ~DATA_SENT;

		if (!is_interactive()) {
			sleep(SLEEP_MODE_PWR_SAVE);
			if (active & UART_ACTIVE) {
				active &= ~UART_ACTIVE;
				if (active & OLED_ACTIVE)
					ossd_sleep(1);
			}
		}
	}
}

void get_rtc_time(char *buf)
{
	uint8_t ts[3];
	rtc_get_time(ts);
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), ts[2], ts[1], ts[0]);
}

void get_vbat(char *buf)
{
	uint16_t vbat = 230 + rd.stat*10;
	sprintf_P(buf, PSTR("Vbat %d.%d"), vbat/100, vbat %100);
}

void print_lsd(void)
{
	char buf[16];
	get_rtc_time(buf);

	uint32_t msec = millis();
	printf_P(PSTR("%s %lu %d.%d\n"), buf, msec, bmp.t, bmp.tdec);
}

void print_rsd(void)
{
	if (!(flags & RDATA_VALID))
		return;

	printf_P(PSTR("%02d:%02d:%02d V %d T %d.%d\n"),
		rd_ts[0], rd_ts[1], rd_ts[2], rd_bv, rd.val, rd.dec);
}

void show_time(char *buf)
{
	get_rtc_time(buf);
	ossd_putlx(2, -1, buf, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
	sprintf_P(buf, PSTR("T %d.%d"), bmp.t, bmp.tdec);
	ossd_putlx(4, -1, buf, OSSD_TEXT_UNDERLINE);
}

void print_status(void)
{
	char buf[16];

	get_vbat(buf);
	printf_P(PSTR("Sensor ID %d, txpwr %ddB, %s\n"), nid, -3*txpwr, buf);
	get_rtc_time(buf);
	printf_P(PSTR("RTC time %s "), buf);
	printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
	print_rsd();
}
