/* Simple SPI interface (no interrupts) for ATmega32

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

#ifndef ATMEGA32_SPI_H
#define ATMEGA32_SPI_H

#include "pinio.h"

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// if there is only one SPI device connected we can set SPI_FAST_WRITE to 1
#ifndef SPI_FAST_WRITE
#define SPI_FAST_WRITE 0
#endif

#define SPI_CLOCK_DIV2   0x10
#define SPI_CLOCK_DIV4   0x00
#define SPI_CLOCK_DIV8   0x11
#define SPI_CLOCK_DIV16  0x01
#define SPI_CLOCK_DIV32  0x12 
#define SPI_CLOCK_DIV64  0x02	
#define SPI_CLOCK_DIV128 0x03

#define SPI_SS  PNB4
#define SPI_SCK PNB7
#define SPI_SDI PNB5
#define SPI_SDO PNB6

// sets SS, SDI and SCK as outputs, selects master mode, and sets spi clock
static inline void spi_init(uint8_t clock)
{
	pinMode(SPI_SS, OUTPUT_HIGH);
	pinMode(SPI_SDI, OUTPUT);
	pinMode(SPI_SCK, OUTPUT);
	// enable SPI master
	SPCR = _BV(SPE) | _BV(MSTR) | (clock & 0x03);
	SPSR = (clock >> 4) & 0x01;

	// dummy write for ILI_SPI_FAST mode
#if SPI_FAST_WRITE
	SPDR = 0;
#endif
}

static inline void spi_set_clock(uint8_t clock)
{
	uint8_t reg = SPCR;
	reg &= ~0x03;
	SPCR = reg | (clock & 0x03);
	SPSR = (clock >> 4) & 0x01;
}

static inline void spi_write_byte(uint8_t data)
{
#if SPI_FAST_WRITE
	while(!(SPSR & _BV(SPIF)));
	SPDR = data;
#else
	SPDR = data;
	while(!(SPSR & _BV(SPIF)));
#endif
}

static inline uint8_t spi_read_byte(uint8_t data)
{
	SPDR = data;
	while(!(SPSR & _BV(SPIF)));
	return SPDR;
}

static inline void spi_write_word(uint16_t data)
{
#if SPI_FAST_WRITE
	while(!(SPSR & _BV(SPIF)));
	SPDR = (data >> 8);
	while(!(SPSR & _BV(SPIF)));
	SPDR = data;
#else
	SPDR = (data >> 8);
	while(!(SPSR & _BV(SPIF)));
	SPDR = data;
	while(!(SPSR & _BV(SPIF)));
#endif
}

#define power_spi_disable() (SPCR &= ~_BV(SPE))
#define power_spi_enable() (SPCR |= _BV(SPE))

#ifdef __cplusplus
}
#endif

#endif

