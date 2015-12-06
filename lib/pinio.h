/*  Simple wrappers for accesing ATmega32 registers

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
#ifndef ATMEGA_PIN_IO_H
#define ATMEGA_PIN_IO_H

#include <avr/io.h>
#include <util/delay.h>

// defines for pinMode()
#define INPUT 	    0
#define INPUT_HIGHZ 0
#define OUTPUT	    1
#define OUTPUT_LOW  1
#define INPUT_UP    2
#define OUTPUT_HIGH 3

#define LOW		0
#define HIGH	1
#define HIGH_Z	0
#define PULLUP	1

// MMR-70 has only some pins of ports B and D populated
#define TP4		PB0 // TP4, T0
#define TP5		PB1 // TP5, T1
#define EXSS    PB4 // an extra pin soldered to SPI SS
#define TP7     PB5	// TP7, MOSI
#define TP3     PB6	// TP3, MISO
#define TP10    PB7	// TP10, SCK

#define EXPC3   PC3 // PC3 if soldered
#define EXPC4   PC4 // PC4 if soldered

#define RDSINT	PD2 // RDSINT
#define EXINT1  PD3 // an extra pin soldered to INT1 (PD3)
#define LED1	PD7 // LED1

// PORTA pins
#define PNA0	0x00
#define PNA1	0x01
#define PNA2	0x02
#define PNA3	0x03
#define PNA4	0x04
#define PNA5	0x05
#define PNA6	0x06
#define PNA7	0x07

// PORTB pins
#define PNB0	0x10
#define PNB1	0x11
#define PNB2	0x12
#define PNB3	0x13
#define PNB4	0x14
#define PNB5	0x15
#define PNB6	0x16
#define PNB7	0x17

// PORTC pins
#define PNC0	0x20
#define PNC1	0x21
#define PNC2	0x22
#define PNC3	0x23
#define PNC4	0x24
#define PNC5	0x25
#define PNC6	0x26
#define PNC7	0x27

// PORTD pins
#define PND0	0x30
#define PND1	0x31
#define PND2	0x32
#define PND3	0x33
#define PND4	0x34
#define PND5	0x35
#define PND6	0x36
#define PND7	0x37

// Analog channels
#define ADC0 0
#define ADC1 1
#define ADC2 2
#define ADC3 3
#define ADC4 4
#define ADC5 5
#define ADC6 6
#define ADC7 7

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// General Port access functions

// manipulates DDRx register
static inline void _pin_dir(volatile uint8_t *port, uint8_t mask, uint8_t dir)
{
	if (dir)
		*port |= mask;
	else
		*port &= ~mask;
}

// manipulates PORTx register
static inline void _pin_set(volatile uint8_t *port, uint8_t mask, uint8_t val)
{
	if (val)
		*port |= mask;
	else
		*port &= ~mask;
}

// manipulates DDRx and PORTx register
static inline void _pin_mode(volatile uint8_t *port, uint8_t mask, uint8_t mode)
{
	if (mode & 0x01)
		*port |= mask;
	else
		*port &= ~mask;
	_pin_set(port + 1, mask, mode & 0x02);
}

// reads PINx register
static inline uint8_t _pin_get(volatile uint8_t *port, uint8_t mask)
{
	return (*port & mask);
}

#define get_pinb(pin) (PINB & _BV(pin))
#define get_pinc(pin) (PINC & _BV(pin))
#define get_pind(pin) (PIND & _BV(pin))

#define set_pinb(pin) (PORTB |= _BV(pin))
#define set_pinc(pin) (PORTC |= _BV(pin))
#define set_pind(pin) (PORTD |= _BV(pin))

#define clear_pinb(pin) (PORTB &= ~_BV(pin))
#define clear_pinc(pin) (PORTC &= ~_BV(pin))
#define clear_pind(pin) (PORTD &= ~_BV(pin))

void pinDir(uint8_t pin, uint8_t dir);   // INPUT-OUTPUT
void pinMode(uint8_t pin, uint8_t mode); // INPUT, INPUT_UP, OUTPUT_LOW, OUTPUT_HIGH

uint8_t digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t val);

// Arduino-type delay
static inline void delay(uint16_t msec)
{
	_delay_ms(msec);
}

// in case if you managed to solder some wires to analogue inputs...
#define VREF_AVCC     _BV(REFS0)
#define VREF_EXTERNAL 0
#define VREF_INTERNAL (_BV(REFS0) | _BV(REFS1))

static inline void analogReference(uint8_t ref)
{
	uint8_t admux = ADMUX;
	admux &= ~(_BV(REFS0) | _BV(REFS1));
	admux |= ref;
	ADMUX = admux;

	// Activate ADC with Prescaler 128 --> 8Mhz/128 = 62.5kHz
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

static inline void analogSetChannel(uint8_t channel)
{
	uint8_t admux = ADMUX;
	admux &= ~0x07;
	admux |= (channel & 0x07);
	ADMUX = admux;
}

static inline uint8_t analogGetChannel(void)
{
	return (ADMUX & 0x07);
}

static inline void analogStart(void)
{
	// Start conversion
	ADCSRA |= _BV(ADSC);
}

static inline void analogStop(void)
{
	// Stop conversion interrupt by reseting ADIF
	ADCSRA &= ~_BV(ADIE);
	ADCSRA |= _BV(ADIE);
}

// Arduino-type analogRead(): synchronous ADC conversion
static inline uint16_t analogRead(uint8_t channel)
{
	uint16_t val;

	// disable interrupt and select 10 bit resolution
	uint8_t adie = ADCSRA & _BV(ADIE);
	ADCSRA &= ~_BV(ADIE);
	uint8_t adlar = ADMUX & _BV(ADLAR);
	ADMUX &= ~_BV(ADLAR);

	// select ADC channel
	analogSetChannel(channel);
	analogStart();
	// wait until conversion completed
	while(ADCSRA & _BV(ADSC));
	val = ADCW;

	// restore interrupt flag and resolution
	ADCSRA |= adie;
	ADMUX  |= adlar;

	// do not disable ADC to avoid "Extended conversion"
	// we disable it only when going to sleep modes
	// ADCSRA &= ~_BV(ADEN);

	return val;
}

#ifdef __cplusplus
}
#endif
#endif
