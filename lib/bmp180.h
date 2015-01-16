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
#ifndef BMP_180_I2C_H
#define BMP_180_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0
} // dummy bracket for Visual Assist
#endif
#endif

#define BMP180_CMD_REG 0xF4

#define BMP180_GET_T 0x2E
#define BMP180_GET_P 0xF4

#define BMP180_T_VALID 0x01
#define BMP180_P_VALID 0x02

typedef struct bmp180_s
{
	// calibration coefficients from E2PROM
	union {
		struct {
			int16_t  ac1, ac2, ac3;
			uint16_t ac4, ac5, ac6;
			int16_t  b1, b2;
			int16_t  mc, md;
		};
		int16_t cc[10];
	};

	uint8_t  cmd;   // BMP180_GET_*
	uint8_t  valid; // BMP180_*_valid

	uint16_t rawt;  // raw temperature
	int8_t   t;     // temperature in C
	uint8_t  tdec;  // temperature decimal 
	int32_t  b6;    // intermediate value for p calculation

	uint32_t rawp;  // raw pressure
	uint16_t p;     // pressure in hPa
	uint8_t  pdec;  // pressure decimal
} bmp180_t;

// read calibration coefficients and sent first request for temperature
int8_t bmp180_init(bmp180_t *pcc);

// process requested data and issue new read request
#define BMP180_P_MODE 0
#define BMP180_T_MODE 1
int8_t bmp180_poll(bmp180_t *pcc, uint8_t mode);

#ifdef __cplusplus
}
#endif
#endif
