/* OLED display support for ATmega32L on MMR-70

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
#ifndef OLED_SSD1306_I2C_H
#define OLED_SSD1306_I2C_H

/**
	Limited set of functions for SSD1306 compatible OLED displays in text mode
	to minimize memory footprint if used on Atmel AVRs with low memory.
*/

#ifdef __cplusplus
extern "C" {
#if 0
} // dummy bracket for Visual Assist
#endif
#endif

/** Target platform */
#define OSSD_AVR	 1 /*< AVR compiler */
#define OSSD_RPI	 2 /*< Raspberry Pi */
#define OSSD_GALILEO 3 /*< Reserved for Intel Galileo */

#ifdef __AVR_ARCH__
	#define OSSD_TARGET OSSD_AVR
#else
	#define OSSD_TARGET OSSD_RPI
#endif

#if (OSSD_TARGET == OSSD_AVR)
	#define I2C_OSSD (0x3C << 1)
#else
	#include <stdint.h>
#endif

/** 
  flat cable connected at the top
  use ossd_init(OSSD_UPDOWN) to rotate screen
  */
#define OSSD_UPDOWN 0x09

/** set default parameters */
int8_t ossd_init(uint8_t orientation);

/** fill screen with specified pattern */
void ossd_fill_screen(uint8_t data);

/** set display to sleep mode */
void ossd_sleep(uint8_t on_off);

/** set display contrast */
void ossd_set_contrast(uint8_t val);

/**
 output string up to 64 chars in length
 line: 0-7
 x:    0-127, or -1 for centre of the line
 str:  output string
 atr:  OSSD_TEXT_*
 */
void ossd_putlx(uint8_t line, int8_t x, const char *str, uint8_t atr);

/** void screen */
static inline void ossd_cls(void) {
	ossd_fill_screen(0);
}

#ifdef __cplusplus
}
#endif

#endif
