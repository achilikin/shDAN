/* Using ATmega32L on MMR-70

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
#ifndef __AVR_MMR70_H__
#define __AVR_MMR70_H__

#include <stdint.h>
#include <avr/eeprom.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// define mask of populated ADC inputs here, 0 if not populated
#define ADC_MASK 0xF0

// debug flags
#define ADC_ECHO 0x01
#define RHT_ECHO 0x02
#define RHT_LOG  0x04

extern char rds_name[9]; // RDS PS name
extern char rds_data[61];  // RDS RT string
extern char fm_freq[17];   // FM frequency
extern char status[17];

// runtime radio flags
#define RADIO_TXPWR0 0x0000
#define RADIO_TXPWR1 0x0001
#define RADIO_TXPWR2 0x0002
#define RADIO_TXPWR3 0x0003 // txpwr mask
#define RADIO_TXPWR  0x0003
#define RADIO_POWER  0x0004
#define RADIO_RDS    0x0008
#define RADIO_MUTE   0x0010
#define RADIO_STEREO 0x0020
#define RADIO_GAIN   0x0040 // turn on -9bD audio gain
#define RADIO_VOLUME 0x0F00 // volume mask

// other runtime flags
#define LOAD_OSCCAL  0x2000
#define RDS_RESET    0x4000
#define RDS_RT_SET   0x8000

extern uint8_t  EEMEM em_rds_name[8];
extern uint16_t EEMEM em_radio_freq;
extern uint16_t EEMEM em_rt_flags;

extern uint16_t radio_freq;
extern uint16_t rt_flags;
extern uint8_t  debug_flags;

extern uint32_t uptime;
extern uint32_t sw_clock;

void   get_tx_pwr(char *buf);

#ifdef __cplusplus
}
#endif
#endif
