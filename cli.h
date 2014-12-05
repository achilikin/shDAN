/* Simple command line parser for ATmega32 on MMR-70

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
#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__

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
#define ADC_ECHO   0x01
#define RHT03_ECHO 0x02

#define CMD_LEN 0x07F // big enough to accommodate RDS text

extern char ns741_name[9]; // RDS PS name
extern char rds_data[61];  // RDS RT string
extern char fm_freq[17];   // FM frequency
extern char status[17];

// runtime flags
#define NS741_TXPWR0 0x0000
#define NS741_TXPWR1 0x0001
#define NS741_TXPWR2 0x0002
#define NS741_TXPWR3 0x0003 // txpwr mask
#define NS741_TXPWR  0x0003
#define NS741_RADIO  0x0004
#define NS741_RDS    0x0008
#define NS741_MUTE   0x0010
#define NS741_STEREO 0x0020
#define NS741_GAIN   0x0040 // turn on -9bD audio gain
#define NS741_VOLUME 0x0F00 // volume mask

#define RDS_RESET    0x4000
#define RDS_RT_SET   0x8000

extern uint8_t  EEMEM em_ns741_name[8];
extern uint16_t EEMEM em_ns741_freq;
extern uint16_t EEMEM em_ns741_flags;

extern uint16_t ns741_freq;
extern uint16_t rt_flags;
extern uint8_t  debug_flags;

void   cli_init(void);
int8_t cli_interact(void *rht);

void     get_tx_pwr(char *buf);

#ifdef __cplusplus
}
#endif
#endif
