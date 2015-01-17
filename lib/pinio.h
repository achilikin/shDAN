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
#ifndef ATMEGA_PIN_IO_H
#define ATMEGA_PIN_IO_H

#include <avr/io.h>
#include <util/delay.h>

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

#define PNB0	0  // TP4, T0
#define PNB1	1  // TP5, T1
#define PNB2	2  // NC
#define PNB3	3  // NC
#define PNB4	4  // SPI SS, if soldered 
#define PNB5	5  // TP7, MOSI
#define PNB6	6  // TP3, MISO
#define PNB7	7  // TP10, SCK

#define PND0	8  // RXD
#define PND1	9  // TXD
#define PND2	10 // RDSINT
#define PND3	11 // INT1, if soldered
#define PND4	12 // NC
#define PND5	13 // NC
#define PND6	14 // NC
#define PND7	15 // LED1

#define PNC0	16 //
#define PNC1	17 //
#define PNC2	18 //
#define PNC3	19 //
#define PNC4	20 //
#define PNC5	21 //
#define PNC6	22 //
#define PNC7	23 //

// define mask of populated ADC inputs here, 0 if not populated
#define ADC_PA0 0x01
#define ADC_PA1 0x02
#define ADC_PA2 0x04
#define ADC_PA3 0x08
#define ADC_PA4 0x10
#define ADC_PA5 0x20
#define ADC_PA6 0x40
#define ADC_PA7 0x80

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// General Port access functions

// manipulates DDRx only
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

// reads PINB register
static inline uint8_t get_pinb(uint8_t pin)
{
	return (PINB & _BV(pin));
}

#define set_pinb(pin) (PORTB |= _BV(pin))
#define set_pind(pin) (PORTD |= _BV(pin))
#define set_pinc(pin) (PORTC |= _BV(pin))

#define clear_pinb(pin) (PORTB &= ~_BV(pin))
#define clear_pind(pin) (PORTD &= ~_BV(pin))
#define clear_pinc(pin) (PORTC &= ~_BV(pin))

// Port access
// *mode(INPUT_HIGHZ) or *mode(OUTPUT_LOW)
// *dir(INPUT) or *dir(OUTPUT)
// *set(LOW|HIGH)
static inline void mmr_tp3_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP3), mode); }
static inline void mmr_tp3_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP3), dir);   }
static inline void mmr_tp3_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP3), val);  }
static inline uint8_t mmr_tp3_get(void)       { return _pin_get(&PINB, _BV(TP3)); }

static inline void mmr_tp4_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP4), mode); }
static inline void mmr_tp4_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP4), dir);   }
static inline void mmr_tp4_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP4), val);  }
static inline uint8_t mmr_tp4_get(void)       { return _pin_get(&PINB, _BV(TP4)); }

static inline void mmr_tp5_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP5), mode); }
static inline void mmr_tp5_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP5), dir);   }
static inline void mmr_tp5_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP5), val);  }
static inline uint8_t mmr_tp5_get(void)       { return _pin_get(&PINB, _BV(TP5)); }

static inline void mmr_tp7_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP7), mode); }
static inline void mmr_tp7_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP7), dir);   }
static inline void mmr_tp7_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP7), val);  }
static inline uint8_t mmr_tp7_get(void)       { return _pin_get(&PINB, _BV(TP7)); }

static inline void mmr_tp10_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP10), mode); }
static inline void mmr_tp10_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP10), dir);   }
static inline void mmr_tp10_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP10), val);  }
static inline uint8_t mmr_tp10_get(void)       { return _pin_get(&PINB, _BV(TP10)); }

static inline void mmr_led_on(void)  { _pin_dir(&DDRD, _BV(LED1), OUTPUT); }
static inline void mmr_led_off(void) { _pin_dir(&DDRD, _BV(LED1), INPUT);  }

static inline void mmr_rdsint_mode(uint8_t mode) {  _pin_mode(&DDRD, _BV(RDSINT), mode); }
static inline void mmr_rdsint_set(uint8_t val)   {  _pin_set(&PORTD, _BV(RDSINT), val);  }
static inline uint8_t mmr_rdsint_get(void)       { return _pin_get(&PIND, _BV(RDSINT));  }

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
	// Stop conversion by reseting ADEN
	ADCSRA &= ~_BV(ADEN);
	ADCSRA |= _BV(ADEN);
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
