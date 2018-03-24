/*  Simple I2C Atmel 24Cxx (up to 24C512) EEPROM support for ATmega32
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
#ifndef I2C_EEPROM_H
#define I2C_EEPROM_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick Visual Assist
}
#endif
#endif

#ifndef I2C_MEM_PAGE_SIZE
#define I2C_MEM_PAGE_SIZE 64 // by default 24C256 - 64 bytes
#endif

#ifndef I2C_MEM_PAGE_SHIFT
#define I2C_MEM_PAGE_SHIFT 6 // by default 24C256 - 6 bits (log2(64))
#endif

#ifndef I2C_MEM_MAX_PAGE
#define I2C_MEM_MAX_PAGE 512 // by default 24C256 - 512 pages
#endif

typedef uint8_t i2cmem_idle_callback(void);

void i2cmem_set_idle_callback(i2cmem_idle_callback *pcall);

int8_t i2cmem_read_data(uint16_t addr, void *dest, uint8_t len);

int8_t i2cmem_write_byte(uint16_t addr, uint8_t data);
int8_t i2cmem_write_data(uint16_t addr, void *src, uint8_t len);
int8_t i2cmem_write_page(uint16_t page, uint8_t offset, void *src, int8_t len);

#ifdef __cplusplus
}
#endif
#endif


