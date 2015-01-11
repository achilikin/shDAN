/* BMP180 digital pressure sensor
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
#include <stdio.h>
#include <string.h>

#include "bmp180.h"
#include "i2cmaster.h"

#define I2C_BMP (0x77 << 1)

static int8_t bmp180_write(uint8_t addr, uint8_t cmd)
{
	if (i2c_start(I2C_BMP | I2C_WRITE) != 0)
		return -1;
	i2c_write(addr);
	i2c_write(cmd);
	i2c_stop();
	return 0;
}

static int8_t bmp180_read_data(bmp180_t *pcc)
{
	if (i2c_start(I2C_BMP | I2C_WRITE) != 0)
		return -1;
	i2c_write(0xF6);
	i2c_stop();

	i2c_start(I2C_BMP | I2C_READ);
	if (pcc->cmd == BMP180_GET_T) {
		uint8_t *buf = (uint8_t *)&pcc->rawt;
		for(int8_t i = 0; i < 2; i++)
			buf[1-i] = i2c_read(!i);
	}
	else {
		uint8_t *buf = (uint8_t *)&pcc->rawp;
		for(int8_t i = 0; i < 3; i++)
			buf[2-i] = i2c_read(i < 2);
		pcc->rawp = pcc->rawp >> 5;
	}
	i2c_stop();
	return 0;
}

static inline uint16_t bswap16(uint16_t val)
{
	uint8_t *pbyte = (uint8_t *)&val;
	uint8_t tmp = pbyte[0];
	pbyte[0] = pbyte[1];
	pbyte[1] = tmp;
	return *(uint16_t *)pbyte;
}

int8_t bmp180_init(bmp180_t *pcc)
{
	uint16_t buf[11];
	memset(pcc, 0, sizeof(bmp180_t));

	if (i2c_start(I2C_BMP | I2C_WRITE) != 0)
		return -1;

	i2c_write(0xAA);
	i2c_stop();
	i2c_start(I2C_BMP | I2C_READ);

	uint8_t *pbuf = (uint8_t *)buf;
	for(int8_t i = 0; i < 22; i++)
		pbuf[i] = i2c_read(i < 21);
	i2c_stop();

	for(uint8_t i = 0; i < 11; i++) {
		int n = i;
		if (i == 8) // skip unused 'MB'
			continue;
		if (n > 7)
			n -= 1;
		pcc->cc[n] = bswap16(buf[i]);
	}

#if 0
	for(int8_t i = 0; i < 10; i++)
		printf(" %04X", press.cc[i]);
	printf("\n");

	printf("a: %d %d %d %u %u %u\n", press.ac1, press.ac2, press.ac3,
		press.ac4, press.ac5, press.ac6);
	printf("b: %d %d\n", press.b1, press.b2);
	printf("m: %d %d\n", press.mc, press.md);
#endif

	pcc->cmd = BMP180_GET_T;
	bmp180_write(BMP180_CMD_REG, pcc->cmd);

	return 0;
}

int8_t bmp180_poll(bmp180_t *pcc, uint8_t t_only)
{
	// read raw t/p from bmp180
	if (bmp180_read_data(pcc) != 0) {
		pcc->valid &= ~(BMP180_T_VALID | BMP180_P_VALID);
		return -1;
	}

	// quite lengthly conversion of raw t/p values
	if (t_only || (pcc->cmd == BMP180_GET_T)) {
		int32_t x1 = (((uint32_t)pcc->rawt - (uint32_t)pcc->ac6)*pcc->ac5) >> 15;
		int32_t x2 = (((int32_t)pcc->mc) << 11)/(x1 + pcc->md);
		int32_t b5 = x1 + x2;
		pcc->b6 = b5 - 4000;
		int16_t t = (int16_t)((b5 + 8) >> 4);
		pcc->t = t / 10;
		pcc->tdec = t % 10;
		pcc->valid |= BMP180_T_VALID;
		pcc->cmd = (t_only) ? BMP180_GET_T : BMP180_GET_P;
	}
	else {
		int32_t x1 = pcc->b2;
		x1 = (x1 * ((pcc->b6 * pcc->b6) >> 12)) >> 11;
		int32_t x2 = pcc->ac2;
		x2 = (x2 * pcc->b6) >> 11;
		int32_t x3 = x1 + x2;
		int32_t b3 = pcc->ac1;
		b3 = ((((b3 << 2) + x3) << 3) + 2) >> 2;
		x1 = pcc->ac3;
		x1 = (x1 * pcc->b6) >> 13;
		x2 = pcc->b1;
		x2 = (x2 * ((pcc->b6*pcc->b6) >> 12)) >> 16;
		x3 = (x1 + x2 +2) >> 2;
		uint32_t b4 = pcc->ac4;
		b4 = (b4 * ((uint32_t)(x3 + 32768))) >> 15;
		uint32_t b7 = (pcc->rawp - b3)*(50000 >> 3);
		int32_t p;
		if (b7 & 0x80000000)
			p = (b7/b4) >> 1;
		else
			p = (b7*2)/b4;
		x1 = p >> 8;
		x1 = x1 * x1;
		x1 = (x1 * 3038) >> 16;
		x2 = (-7357 * p) >> 16;
		p = p + ((x1 + x2 + 3791) >> 4);
		pcc->p = (uint16_t)(p / 100);
		pcc->pdec = (uint8_t)(p % 100);
		pcc->valid |= BMP180_P_VALID;
		pcc->cmd = BMP180_GET_T;
	}

	return bmp180_write(BMP180_CMD_REG, pcc->cmd);
}
