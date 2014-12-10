/* Real Time Clock PCF2127AT support for ATmega32L on MMR-70
   http://www.nxp.com/documents/data_sheet/PCF2127.pdf

   Copyright (c) 2014 Andrey Chilikin (https://github.com/achilikin)

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
#include "pcf2127.h"
#include "i2cmaster.h"

#define I2C_RTC (0x51 << 1)


int8_t pcf2127_write(uint8_t addr, uint8_t *buf, uint8_t len)
{
	if (i2c_start(I2C_RTC | I2C_WRITE) != 0)
		return -1;

	i2c_write(addr);
	for(int8_t i = 0; i < len; i++)
		i2c_write(buf[i]);

	i2c_stop();
	return 0;
}


int8_t pcf2127_read(uint8_t addr, uint8_t *buf, uint8_t len)
{
	if (i2c_start(I2C_RTC | I2C_WRITE) != 0)
		return -1;
	i2c_write(addr);
	i2c_stop();

	i2c_start(I2C_RTC | I2C_READ);
	for(int8_t i = 0; i < len; i++)
		buf[i] = i2c_read(i < (len - 1));
	i2c_stop();

	return 0;
}

int8_t pcf2127_init(void)
{
	uint8_t buf[8];
	
	// clear control registers
	for(uint8_t i = 0; i < 3; i++) buf[i] = 0x00;
	pcf2127_write(PCF_REG_CTL1, buf, 3);
	// clear watchdog registers
	pcf2127_write(PCF_REG_WDGCTL, buf, 2);
	// clear alarm registers
	for(uint8_t i = 0; i < 5; i++) buf[i] = 0x80;
	pcf2127_write(PCF_REG_ASEC, buf, 5);

	// 1 minute temperature compensation, disable clock out
	buf[0] = 0x87;
	pcf2127_write(PCF_REG_CLKOUT, buf, 1);
	buf[0] = 0xA6; // refresh OTP
	pcf2127_write(PCF_REG_CLKOUT, buf, 1);
	// disable time stamping
	buf[0] = 0x40;
	pcf2127_write(PCF_REG_TSCTL, buf, 1);

	return 0;
}

static inline uint8_t bcd_to_dec(uint8_t bcd)
{
	return (bcd & 0x0F) + (bcd >> 4) * 10;
}

int8_t pcf2127_get_time(pcf_td_t *ptd)
{
	uint8_t buf[3];
	uint8_t *ptm = (uint8_t *)ptd;

	if (pcf2127_read(PCF_REG_SEC, buf,  3) == 0) {
		for(int8_t i = 0; i < 3; i++)
			ptm[i] = bcd_to_dec(buf[2 - i]);
		return 0;
	}

	return -1;
}

int8_t pcf2127_get_date(pcf_td_t *ptd)
{
	uint8_t buf[7];
	uint8_t *ptm = (uint8_t *)ptd;

	if (pcf2127_read(PCF_REG_SEC, buf,  7) == 0) {
		for(int8_t i = 0; i < 3; i++)
			ptm[i] = bcd_to_dec(buf[2 - i]);
		ptd->day   = bcd_to_dec(buf[3]);
		ptd->month = bcd_to_dec(buf[5]);
		ptd->year  = bcd_to_dec(buf[6]);
		return 0;
	}

	return -1;
}

static int8_t pcf_start_ram(uint16_t addr, uint8_t len)
{
	if ((addr + len) > PCF_RAM_SIZE)
		return -1;

	uint8_t buf[2];
	buf[0] = (uint8_t)((addr >> 8) & 0x01);
	buf[1] = (uint8_t)(addr & 0xFF);

	return pcf2127_write(PCF_REG_MSBRAM, buf, 2);
}

int8_t pcf2127_ram_write(uint16_t addr, uint8_t *data, uint8_t len)
{
	if (pcf_start_ram(addr, len) == 0)
		return pcf2127_write(PCF_REG_WRAM, data, len);
	return -1;
}

int8_t pcf2127_ram_read(uint16_t addr, uint8_t *data, uint8_t len)
{
	if (pcf_start_ram(addr, len) == 0)
		return pcf2127_read(PCF_REG_RRAM, data, len);
	return -1;
}

int8_t pcf2127_set_clkout(uint8_t hz)
{
	uint8_t reg;
	hz &= PCF_CLKOUT_OFF;
	
	pcf2127_read(PCF_REG_CLKOUT, &reg, 1);
	reg &= ~PCF_CLKOUT_OFF;
	reg |= hz;
	return pcf2127_write(PCF_REG_CLKOUT, &reg, 1);
}
