/* Test app for ILI9225 based display on ATmega32L

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
#ifndef ILI_TEST_MAIN_H
#define ILI_TEST_MAIN_H

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

extern uint8_t EEMEM em_osccal;
extern uint8_t EEMEM em_flags;

#define ACTIVE_DLED 0x01

extern uint8_t active;
extern uint8_t rt_flags;

extern uint32_t uptime;
extern uint32_t swtime;

void print_status(void);
void get_time(char *buf); // convert seconds to hh:mm:ss 24h format

int8_t cli_ili(char *buf, void *ptr);

#ifdef __cplusplus
}
#endif
#endif
