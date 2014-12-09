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
#ifndef __RTC_PFC2127_I2C__
#define __RTC_PFC2127_I2C__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0
} // dummy bracket for Visual Assist
#endif
#endif

#define PCF_REG_CTL1   0x00
// CTL1 bits
#define PCF_CTL1_TEST  0x80
#define PCF_CTL1_STOP  0x40
#define PCF_CTL1_TSF   0x10
#define PCF_CTL1_POR   0x08
#define PCF_CTL1_AMPM  0x04
#define PCF_CTL1_MI    0x02
#define PCF_CTL1_SI    0x01

#define PCF_REG_CTL2   0x01
#define PCF_REG_CTL3   0x02

#define PCF_REG_SEC    0x03
#define PCF_REG_MIN    0x04
#define PCF_REG_HOUR   0x05
#define PCF_HOUR_PM    0x20

#define PCF_REG_DAY    0x06
#define PCF_REG_WDAY   0x07
#define PCF_REG_MONTH  0x08
#define PCF_REG_YEAR   0x09

#define PCF_REG_ASEC   0x0A
#define PCF_REG_AMIN   0x0B
#define PCF_REG_AHOUR  0x0C
#define PCF_REG_ADAY   0x0D
#define PCF_REG_AWDAY  0x0E

#define PCF_REG_CLKOUT 0x0F
#define PCF_CLKOUT_32K 0x00
#define PCF_CLKOUT_16K 0x01
#define PCF_CLKOUT_08K 0x02
#define PCF_CLKOUT_04K 0x03
#define PCF_CLKOUT_02K 0x04
#define PCF_CLKOUT_01K 0x05
#define PCF_CLKOUT_1HZ 0x06
#define PCF_CLKOUT_OFF 0x07

#define PCF_REG_WDGCTL 0x10
#define PCF_REG_WDGVAL 0x11

#define PCF_REG_TSCTL  0x12
#define PCF_REG_TSSEC  0x13
#define PCF_REG_TSMIN  0x14
#define PCF_REG_TSHOUR 0x15
#define PCF_REG_TSDAY  0x16
#define PCF_REG_TSMON  0x17
#define PCF_REG_TSYEAR 0x18

#define PCF_REG_AGING  0x19

#define PCF_REG_MSBRAM 0x1A
#define PCF_REG_LSBRAM 0x1B
#define PCF_REG_WRAM   0x1C
#define PCF_REG_RRAM   0x1D
#define PCF_RAM_SIZE   512

#define PCF_MAX_REG    (PCF_REG_LSBRAM + 1)

#define PCF_TIME_24H   0x00
#define PCF_TIME_12H   PCF_CTL1_AMPM

typedef struct pcf_td_s
{
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t year;
	uint8_t month;
	uint8_t day;
} pcf_td_t;

int8_t pcf2127_init(void); // clear all flags and alarm
int8_t pcf2127_read(uint8_t addr, uint8_t *buf, uint8_t len);
int8_t pcf2127_write(uint8_t addr, uint8_t *buf, uint8_t len);

int8_t pcf2127_ram_read(uint16_t addr, uint8_t *buf, uint8_t len);
int8_t pcf2127_ram_write(uint16_t addr, uint8_t *buf, uint8_t len);

int8_t pcf2127_get_time(pcf_td_t *ptd); // reads only time to the ptd
int8_t pcf2127_get_date(pcf_td_t *ptd); // reads time+date to the ptd

int8_t pcf2127_set_clkout(uint8_t hz); // one of PCF_CLKOUT_* above
#ifdef __cplusplus
}
#endif
#endif
