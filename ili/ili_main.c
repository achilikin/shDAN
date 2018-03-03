/* Test app for ILI9225 based display on ATmega32L

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
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "spi.h"
#include "mmrio.h"
#include "timer.h"
#include "serial.h"
#include "bmfont.h"
#include "ili9225.h"
#include "serial_cli.h"

#include "ili_main.h"

#ifndef F_CPU
#error F_CPU must be defined in Makefile, use -DF_CPU=xxxUL
#endif

// some default variables we want to store in EEPROM
// average value from serial_calibrate()
// for MMR70 I'm running this code on it is 168 for 115200, 181 for 38400
#ifndef DEF_OSCCAL
#define DEF_OSCCAL 181 // average value from serial_calibrate()
#endif
uint8_t EEMEM em_osccal = DEF_OSCCAL;

// runtime flags
uint8_t EEMEM em_flags = LOAD_OSCCAL;

uint8_t  active = ACTIVE_DLED;
uint8_t  rt_flags;
uint32_t uptime;
uint32_t swtime;

#define CLOCK_TYPE CLOCK_MILLIS

void test_draw(ili9225_t *ili, uint8_t reset);

uint16_t get_random_color(void)
{
	return RGB16(rand()%256,rand()%256,rand()%256);
}

int main(void)
{
	mmr_led_on(); // turn on LED while booting
	sei();

	// initialise all components
	rt_flags = eeprom_read_byte(&em_flags);
	serial_init(UART_BAUD_RATE);

	if (rt_flags & LOAD_OSCCAL) {
		uint8_t new_osccal = eeprom_read_byte(&em_osccal);
		serial_set_osccal(new_osccal);
	}

	// if 32kHz crystal is attached make sure RTC is up and running 
	if (CLOCK_TYPE & CLOCK_RTC)
		delay(1000);

	// setup our ~millisecond timer for mill*() and tenth_clock counter
	// and enable PWM timer 0
	init_time_clock(CLOCK_TYPE | CLOCK_PWM);
	// reset our soft clock
	uptime = swtime = 0;

	print_status();
	analogReference(VREF_AVCC);

	cli_init();
	// init SPI for ILI9225 display
	spi_init(SPI_CLOCK_DIV2);

	// initialize ili9225 pins
	ili9225_t ili;
	ili.flags = ILI_LED_PIN | ILI_LED_PWM;
	ili.cs  = PNC3;
	ili.rs  = PND4;
	ili.rst = PNC4;
	ili.led = PNB3;

	ili9225_init(&ili);
	// set display position - cable at the top
	ili9225_set_dir(&ili, ILI9225_DISP_UPDOWN);
	ili9225_set_bk_color(&ili, RGB16_RED);

	// measure display clear time
	uint32_t clock = millis();
	ili9225_clear(&ili);
	clock = millis() - clock;
	printf("fill %lu msec\n", clock);
	// restore black color	
	ili9225_set_bk_color(&ili, RGB16_BLACK);
	ili9225_clear(&ili);

	// text output functions
	bmfont_select(BMFONT_8x8);
	clock = millis();
	ili9225_text(&ili, 0, 0, "text text text text ", 0);
	clock = millis() - clock;
	printf("text %lu msec\n", clock);

	bmfont_select(BMFONT_8x16);
	clock = millis();
	ili9225_text(&ili, 0, 10, "text text text text ", TEXT_OVERLINE | TEXT_UNDERLINE);
	clock = millis() - clock;
	printf("text %lu msec\n", clock);

	ili9225_set_fg_color(&ili, RGB16_RED);
	
	mmr_led_off();

	uint8_t led = 0;
	uint16_t pixels = ILI9225_LCD_WIDTH;
	pixels *= ILI9225_LCD_HEIGHT;

	for(;;) {
		cli_interact(cli_ili, &ili);
		// once-a-second checks
		if (tenth_clock >= 10) {
			led ^= 0x02;
			// instead of mmr_led_on()/mmr_led_off()
			if (active & ACTIVE_DLED)
				pinMode(PND7, OUTPUT | led);
			tenth_clock = 0;
			uptime++;
			swtime++;
			if (swtime == 86400)
				swtime = 0;
		}
	}
}

void get_time(char *buf)
{
	uint8_t ts[3];
	ts[2] = uptime / 3600;
	ts[1] = (uptime / 60) % 60;
	ts[0] = uptime % 60;
	sprintf_P(buf, PSTR("%02d:%02d:%02d"), ts[2], ts[1], ts[0]);
}

void print_status(void)
{
	char buf[16];
	get_time(buf);
	printf_P(PSTR("SW time %s "), buf);
	printf_P(PSTR("Uptime %lu sec or %lu:%02ld:%02ld\n"), uptime, uptime / 3600, (uptime / 60) % 60, uptime % 60);
}

void test_draw(ili9225_t *ili, uint8_t reset)
{
	static uint8_t test = 0;
	static uint8_t x1 = 0, x2 = ILI9225_MAX_X;
	static uint8_t y1 = 0, y2 = ILI9225_MAX_Y;

	if (reset) {
		x1 = 0, x2 = ILI9225_MAX_X;
		y1 = 0, y2 = ILI9225_MAX_Y;
	}

	for(;;)	{
		ili9225_set_fg_color(ili, get_random_color());
		if (test == 0) {
			ili9225_rectangle(ili, x1, y1, x2, y2);
			if (x2 >= 1) {
				x2--;
				y2--;
			}
			else{
				x1 = 0, x2 = ILI9225_MAX_X;
				y1 = 0, y2 = ILI9225_MAX_Y;
				break;
			}
		}
	}
}
