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

#include "timer.h"
#include "i2cmem.h"
#include "i2cmaster.h"

#define I2C_MEM (0x50 << 1)

#define I2C_MEM_WRITE_CYCLE   10
#define I2C_MEM_WRITE_CYCLE_B 5
#define I2C_MEM_TIMEOUT I2C_MEM_WRITE_CYCLE_B

static i2cmem_idle_callback *pidle;

void i2cmem_set_idle_callback(i2cmem_idle_callback *pcall)
{
	pidle = pcall;
}

static int8_t i2cmem_ack_poll(uint8_t op, uint8_t tout)
{
	uint8_t ms = mill8();
	do {
		if (pidle)
			pidle();
		if (i2c_start(I2C_MEM | op) == 0)
			return 0;
	} while((mill8() - ms) < tout);

	return -1;
}

int8_t i2cmem_read_data(uint16_t addr, void *dest, uint8_t len)
{
	if (!len || i2cmem_ack_poll(I2C_WRITE, I2C_MEM_TIMEOUT) != 0)
		return -1;
	len--;
	i2c_write(addr >> 8);
	i2c_write(addr);
	i2c_start(I2C_MEM | I2C_READ);
	uint8_t *data = dest;
	for(uint8_t i = 0; i <= len; i++)
		data[i] = i2c_read(len - i);
	i2c_stop();

	return 0;
}

int8_t i2cmem_write_byte(uint16_t addr, uint8_t data)
{
	if (i2cmem_ack_poll(I2C_WRITE, I2C_MEM_TIMEOUT) != 0)
		return -1;
	i2c_write(addr >> 8);
	i2c_write(addr);
	i2c_write(data);
	i2c_stop();

	return 0;
}

int8_t i2cmem_write_page(uint16_t page, uint8_t offset, void *src, int8_t len)
{
	offset &= (I2C_MEM_PAGE_SIZE - 1);
	page = (page << I2C_MEM_PAGE_SHIFT) + offset;

	if ((len + offset) > I2C_MEM_PAGE_SIZE)
		len -= (len + offset) - I2C_MEM_PAGE_SIZE;

	if (len) {
		if (i2cmem_ack_poll(I2C_WRITE, I2C_MEM_TIMEOUT) != 0)
			return -1;
		i2c_write(page >> 8);
		i2c_write(page);
		uint8_t *data = src;
		for(int8_t i = 0; i < len; i++)
			i2c_write(data[i]);

		i2c_stop();
	}

	return len;
}

int8_t i2cmem_write_data(uint16_t addr, void *src, uint8_t len)
{
	uint8_t *data = src;
	uint16_t page = addr >> I2C_MEM_PAGE_SHIFT;
	uint8_t offset = addr & (I2C_MEM_PAGE_SIZE - 1);

	do {
		uint8_t n = I2C_MEM_PAGE_SIZE - offset;
		if (n > len)
			n = len;
		if (i2cmem_write_page(page, offset, data, n) != 0)
			return -1;
		len -= n;
		data += n;
		offset = 0;
		page++;
	} while(len);

	return 0;
}
