/* Using ATmega32L on MMR-70

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

// runtime flags
#define LOAD_OSCCAL  0x01
#define LDATA_VALID  0x02 // local sensor data valid
#define RDATA_VALID  0x04 // remote sensor data valid
#define DATA_SENT    0x08
#define DATA_POLL    0x10
#define DATA_INIT    0x20
#define LSD_ECHO     0x40 // local sensor data log
#define RSD_ECHO     0x80 // remote sensor data log

// active components
#define OLED_ACTIVE  0x20
#define DLED_ACTIVE  0x40
#define UART_ACTIVE  0x80

extern uint8_t EEMEM em_nid;
extern uint8_t EEMEM em_txpwr;
extern uint8_t EEMEM em_osccal;
extern uint8_t EEMEM em_flags;

extern uint8_t nid;
extern uint8_t txpwr;
extern uint8_t flags;
extern uint8_t active;

extern uint32_t uptime;

void print_lsd(void); // print local sensor data
void print_rsd(void); // print remote sensor data
void print_status(void);

void get_vbat(char *buf);
void get_rtc_time(char *buf); // fill buffer with rtc time

uint8_t sht1x_crc(uint8_t data, uint8_t seed);

#ifdef __cplusplus
}
#endif
#endif
