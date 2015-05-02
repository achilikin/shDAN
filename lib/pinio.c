/*  Simple wrappers for accesing ATmega32 registers populated on MMR-70

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
#include "pinio.h"

static volatile uint8_t *ports[4] = { &PORTA, &PORTB, &PORTC, &PORTD };

void pinDir(uint8_t pin, uint8_t dir)
{
	volatile uint8_t *port = ports[pin >> 4];
	uint8_t mask = 1 << (pin & 0x07);
	
	_pin_dir(port - 2, mask, dir);
}

void pinMode(uint8_t pin, uint8_t mode)
{
	volatile uint8_t *port = ports[pin >> 4];
	uint8_t mask = 1 << (pin & 0x07);
	if (mode & 0x01)
		*(port - 1) |= mask;
	else
		*(port - 1) &= ~mask;
	_pin_set(port, mask, mode & 0x02);
}

uint8_t digitalRead(uint8_t pin)
{
	volatile uint8_t *port = ports[pin >> 4] - 2;
	if (*port & (1 << (pin & 0x07)))
		return 1;
	return 0;
}

// pin - one of PNB0-PND7
void digitalWrite(uint8_t pin, uint8_t val)
{
	volatile uint8_t *port = ports[pin >> 4];
	uint8_t mask = 1 << (pin & 0x07);
	if (val)
		*port |= mask;
	else
		*port &= ~mask;
}
