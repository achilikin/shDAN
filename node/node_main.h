/* Data Node for data acquisition network

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
#ifndef NODE_MAIN_H
#define NODE_MAIN_H

#include <stdint.h>
#include <avr/eeprom.h>

#include "dnode.h"

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

// active components
#define UART_ACTIVE  0x80
#define OLED_ACTIVE  0x40
#define DLED_ACTIVE  0x20

extern uint8_t EEMEM em_nid;
extern uint8_t EEMEM em_txpwr;
extern uint8_t EEMEM em_osccal;
extern uint8_t EEMEM em_flags;

extern uint8_t nid;
extern uint8_t txpwr;
extern uint8_t flags;
extern uint8_t active;

extern uint32_t uptime;

void get_rtc_time(char *buf); // fill buffer with rtc time

void print_dval(dnode_t *dval);
void print_status(dnode_t *val);
void get_vbat(dnode_t *val, char *buf);

int8_t cli_node(char *buf, void *ptr);

#ifdef __cplusplus
}
#endif
#endif
