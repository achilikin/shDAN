/*  Simple wrapper for reading serial port (similar to getch) on ATmega32L

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
#ifndef SERIAL_CHAR_H
#define SERIAL_CHAR_H

#include <stdint.h>

#include "uart.h" 

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// non-character flag
#define EXTRA_KEY   0x0100

// support for arrow keys for very simple one command deep history
#define ARROW_UP    0x0141
#define ARROW_DOWN  0x0142
#define ARROW_RIGHT 0x0143
#define ARROW_LEFT  0x0144

#define KEY_HOME    0x0101
#define KEY_INS	    0x0102
#define KEY_DEL	    0x0103
#define KEY_END	    0x0104
#define KEY_PGUP    0x0105
#define KEY_PGDN    0x0106

// default baud rate
#define UART_BAUD_RATE 38400LL // 38400 at 8MHz gives only 0.2% errors

void     serial_init(long baud_rate);
uint16_t serial_getc(void);

#define serial_putc(x) uart_putc((uint8_t)x)
#define serial_wait_sending() for(uint8_t i = 0; UCSRB & _BV(UDRIE); i++)

extern uint8_t osccal_def;

uint8_t serial_set_osccal(uint8_t osccal);
void serial_calibrate(uint8_t osccal);

#ifdef __cplusplus
}
#endif

#endif
