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
uint8_t EEMEM em_tsync = 20; // time sync interval every 20 sessions

// RFM12B sync pattern, better keep it to default 0xD4
// as previous versions of RFM12 do not support anything else
uint8_t EEMEM em_rfm_sync = 0xD4;

uint8_t EEMEM em_rt_flags = LOAD_OSCCAL; // runtime flags

// I/O pins
#define PIN_INTERACTIVE PB0 // on port B
#define REPLY_TIMEOUT 60 // time sync reply timeout, must be less than 127 msec

#define TIME_TO_POLL(x) (rtc_sec == ((x - 1)*5))

uint8_t  rt_flags;
uint8_t  active;
uint32_t uptime;
uint8_t  tsync;
bmp180_t bmp;

uint8_t  nid;   // node id
uint8_t  txpwr; // RFM12 TX power

// List all attached sensors here
int8_t poll_bmp180(dnode_t *dval, void *ptr);
dsens_t sens[] = { {SET_SENS(1,SENS_TEMPER), poll_bmp180, &bmp} };

// power save functions
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
int8_t sync_time(dnode_t *dval)
{
	if (dval)
		rfm12_send(dval, sizeof(dnode_t));

	rfm12_set_mode(RFM_MODE_RX);
	rfm12_reset_fifo();
	rfm12_cmdrw(RFM12CMD_STATUS); // clear any interrupts

	dnode_t tsync;
	uint8_t msec = mill8();
	for(uint8_t dt = 0; dt < REPLY_TIMEOUT; dt = mill8() - msec) {
		if (rfm12_receive_data(&tsync, sizeof(dnode_t), rt_flags & RT_RX_ECHO) == sizeof(dnode_t)) {
			ts_unpack(&tsync);
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
	dnode_t dval; // data node value
	uint8_t isync;

	mmr_led_on(); // turn on LED while booting

	uptime = 0;
	active = 0;
	MCUCSR |= _BV(JTD); // disable JTag if enabled in the fuses

	sei();

	nid = eeprom_read_byte(&em_nid); // our node id
	tsync = eeprom_read_byte(&em_tsync);
	txpwr = eeprom_read_byte(&em_txpwr);
	rt_flags = eeprom_read_byte(&em_rt_flags);

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
	if (rt_flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}
	
	isync = eeprom_read_byte(&em_rfm_sync);
	rfm12_init(isync, RFM12_BAND_868, 868.0, RFM12_BPS_9600);
	isync = 0;
	rfm12_set_txpwr(txpwr);
	mmr_led_off();

	// create "List of Sensors" message
	dval.nid = nid | NODE_TSYNC | SENS_LIST;
	dval.data[0] = 0;
	dval.data[1] = 0;
	dval.data[2] = 0;

	for(uint8_t n = 0; n < sizeof(sens)/sizeof(sens[0]); n++) {
		set_sens_type(&dval, sens[n].tos_sid & 0x0F, sens[n].tos_sid >> 4);
	}

	// try to get RTC time from the base
	for(uint8_t i = 0; i < 5; i++) {
		if (sync_time(&dval) != -1)
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
	rt_flags |= RT_DATA_INIT;
	dval.stat = rfm12_battery(RFM_MODE_IDLE, 14);
	mmr_led_off();

	if (is_interactive()) {
		active |= UART_ACTIVE;
		print_status(&dval);
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
				dval.stat = rfm12_battery(RFM_MODE_IDLE, 14);
				uart_puts_p(PSTR("\n> "));
				active |= UART_ACTIVE;
			}
			cli_interact(cli_node, &dval);
		}

		// once-a-second checks
		if (tenth_clock >= 10) {
			tenth_clock = 0;
			uptime++;
		}

		// poll attached sensors once a minute depending on Node ID
		if ((rt_flags & (RT_DATA_POLL | RT_DATA_INIT)) || (TIME_TO_POLL(nid) && !(rt_flags & RT_DATA_SENT))) {
			awake();
			dval.stat &= ~(STAT_LED | STAT_SLEEP);
			if (active & DLED_ACTIVE)
				dval.stat |= STAT_LED;
			if (!(active & UART_ACTIVE))
				dval.stat |= STAT_SLEEP;

			for(uint8_t n = 0; n < sizeof(sens)/sizeof(sens[0]); n++) {
				if (active & DLED_ACTIVE)
					mmr_led_on();

				if (sens[n].poll(&dval, sens[n].data) == 0)
					dval.nid = SET_NID(nid, n+1);

				if (active & DLED_ACTIVE)
					mmr_led_off();

				// Local Sensor Data log
				if ((rt_flags & (RT_LSD_ECHO | RT_DATA_POLL)) && (active & UART_ACTIVE))
					print_dval(&dval);

				if (n == (sizeof(sens)/sizeof(sens[0]) - 1)) {
					dval.stat |= STAT_EOS;
					if (isync == tsync) {
						dval.nid |= NODE_TSYNC; // request time sync
						dval.stat &= ~STAT_VBAT; // update battery voltage
						dval.stat |= rfm12_battery(RFM_MODE_IDLE, 14) & STAT_VBAT;
					}
				}

				rfm12_send(&dval, sizeof(dval));

				// get time sync reply
				if (isync == tsync) {
					sync_time(NULL);
					isync = 0;
				}
			}

			rfm12_set_mode(RFM_MODE_SLEEP);
			rt_flags |= RT_DATA_SENT;
			isync++;

			if (is_interactive() && (active & OLED_ACTIVE)) {
				show_time(buf);
				get_vbat(&dval, buf);
				ossd_putlx(6, -1, buf, 0);
			}

			serial_wait_sending();
			rt_flags &= ~(RT_DATA_POLL | RT_DATA_INIT);
		}

		if (!TIME_TO_POLL(nid))
			rt_flags &= ~RT_DATA_SENT;

		if (!is_interactive()) {
			if (active & UART_ACTIVE) {
				active &= ~UART_ACTIVE;
				if (active & OLED_ACTIVE)
					ossd_sleep(1);
			}
			sleep(SLEEP_MODE_PWR_SAVE);
		}
	}
}

void get_rtc_time(char *buf)
{
	uint8_t ts[3];
	rtc_get_time(ts);
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), ts[2], ts[1], ts[0]);
}

void get_vbat(dnode_t *val, char *buf)
{
	uint16_t vbat = 230 + (val->stat & STAT_VBAT)*10;
	sprintf_P(buf, PSTR("Vbat %d.%d"), vbat/100, vbat %100);
}

void print_dval(dnode_t *dval)
{
	char buf[16];
	get_rtc_time(buf);

	uint32_t msec = millis();
	printf_P(PSTR("%s %lu %d.%d\n"), buf, msec, dval->val, dval->dec);
}

void show_time(char *buf)
{
	get_rtc_time(buf);
	ossd_putlx(2, -1, buf, OSSD_TEXT_OVERLINE | OSSD_TEXT_UNDERLINE);
	sprintf_P(buf, PSTR("T %d.%d"), bmp.t, bmp.tdec);
	ossd_putlx(4, -1, buf, OSSD_TEXT_UNDERLINE);
}

void print_status(dnode_t *val)
{
	char buf[16];

	get_vbat(val, buf);
	printf_P(PSTR("Sensor ID %d, txpwr %ddB, %s\n"), nid, -3*txpwr, buf);
	get_rtc_time(buf);
	printf_P(PSTR("RTC time %s "), buf);
	printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
}

int8_t poll_bmp180(dnode_t *dval, void *ptr)
{
	bmp180_t *bmp = ptr;
	bmp180_poll(bmp, BMP180_T_MODE);

	if (bmp->valid & BMP180_T_VALID) {
		dval->val = bmp->t;
		dval->dec = bmp->tdec;
		return 0;
	}
	return -1;
}
