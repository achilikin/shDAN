/* Base Station for data acquisition network

   Copyright (c) 2018 Andrey Chilikin (https://github.com/achilikin)

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
#ifndef BASE_MAIN_H
#define BASE_MAIN_H

#include <stdint.h>
#include <avr/eeprom.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

extern char rds_name[9];  // RDS PS name
extern char rds_data[61]; // RDS RT string
extern char fm_freq[17];  // FM frequency
extern char hpa[17];      // pressure in hPa
extern char status[33];   // TxPwr status

// ns741 power flags
#define NS741_TXPWR0 0x00
#define NS741_TXPWR1 0x01
#define NS741_TXPWR2 0x02
#define NS741_TXPWR3 0x03
#define NS741_TXPWR  0x03 // txpwr mask
#define NS741_POWER  0x04
#define NS741_GAIN   0x08 // turn on -9bD audio gain
#define NS741_VOLUME 0xF0 // volume mask

// ns741 runtime flags
#define NS741_RDS    0x01
#define NS741_MUTE   0x02
#define NS741_STEREO 0x04

// runtime flags
#define RT_LOAD_OSCCAL  0x01
#define RT_ECHO_DAN  0x10 // data acquisition node log
#define RT_ECHO_RHT  0x20
#define RT_ECHO_LOG  0x40
#define RT_ECHO_RX   0x80

extern uint8_t  EEMEM em_rds_name[8];
extern uint16_t EEMEM em_radio_freq;
extern uint8_t  EEMEM em_ns_rt_flags;
extern uint8_t  EEMEM em_ns_pwr_flags;
extern uint8_t  EEMEM em_rt_flags;
extern uint8_t  EEMEM em_osccal;
extern uint16_t EEMEM em_nreset;

extern uint16_t radio_freq;
extern uint8_t  ns_rt_flags;
extern uint8_t  ns_pwr_flags;

extern uint8_t  rt_flags;

extern uint32_t uptime;
extern uint32_t sw_clock;

void   get_tx_pwr(char *buf);  // get current NS741 tx power
void   get_fm_freq(char *buf); // get current NS741 FM frequency
void   print_rd(void); // print remote sensor data
int8_t print_rtc_time(void);
void   print_node(uint8_t nid);
void   print_status(uint8_t verbose);
void   update_radio_status(void);

uint8_t io_handler(void); // check if I/O request is pending

int8_t cli_base(char *buf, void *rht);

#define MAX_NODES_PER_SCREEN 8

#define TEXT_CENTRE 0xFF
void putlx(uint8_t line, uint8_t x, const char *str, uint8_t atr);

#ifdef __cplusplus
}
#endif
#endif
