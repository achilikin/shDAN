/*	Simple wrappers for accesing ATmega32 registers populated on MMR-70
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

#include <avr/io.h>

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
#define TP7     PB5	// TP7, MOSI
#define TP3     PB6	// TP3, MISO
#define TP10    PB7	// TP10, SCK

#define RDSINT	PD2 // RDSINT
#define LED1	PD7 // LED1

#define PNB0	0  // TP4, T0
#define PNB1	1  // TP5, T1
#define PNB2	2  // NC
#define PNB3	3  // NC
#define PNB4	4  // NC 
#define PNB5	5  // TP7, MOSI
#define PNB6	6  // TP3, MISO
#define PNB7	7  // TP10, SCK

#define PND0	8  // RXD
#define PND1	9  // TXD
#define PND2	10 // RDSINT
#define PND3	11 // NC
#define PND4	12 // NC
#define PND5	13 // NC
#define PND6	14 // NC
#define PND7	15 // LED1

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

// General Port access functions

// manipulates DDRx only
inline void _pin_dir(volatile uint8_t *port, uint8_t mask, uint8_t dir)
{
	if (dir)
		*port |= mask;
	else
		*port &= ~mask;
}

// manipulates PORTx register
inline void _pin_set(volatile uint8_t *port, uint8_t mask, uint8_t val)
{
	if (val)
		*port |= mask;
	else
		*port &= ~mask;
}

// manipulates DDRx and PORTx register
inline void _pin_mode(volatile uint8_t *port, uint8_t mask, uint8_t mode)
{
	if (mode & 0x01)
		*port |= mask;
	else
		*port &= ~mask;
	_pin_set(port + 1, mask, mode & 0x02);
}

// reads PINx register
inline uint8_t _pin_get(volatile uint8_t *port, uint8_t mask)
{
	return (*port & mask);
}

// Port B pins access
inline void mmr_tp3_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP3), mode); }
inline void mmr_tp3_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP3), dir);   }
inline void mmr_tp3_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP3), val);  }
inline uint8_t mmr_tp3_get(void)       { return _pin_get(&PINB, _BV(TP3)); }

inline void mmr_tp4_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP4), mode); }
inline void mmr_tp4_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP4), dir);   }
inline void mmr_tp4_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP4), val);  }
inline uint8_t mmr_tp4_get(void)       { return _pin_get(&PINB, _BV(TP4)); }

inline void mmr_tp5_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP5), mode); }
inline void mmr_tp5_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP5), dir);   }
inline void mmr_tp5_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP5), val);  }
inline uint8_t mmr_tp5_get(void)       { return _pin_get(&PINB, _BV(TP5)); }

inline void mmr_tp7_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP7), mode); }
inline void mmr_tp7_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP7), dir);   }
inline void mmr_tp7_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP7), val);  }
inline uint8_t mmr_tp7_get(void)       { return _pin_get(&PINB, _BV(TP7)); }

inline void mmr_tp10_mode(uint8_t mode) { _pin_mode(&DDRB, _BV(TP10), mode); }
inline void mmr_tp10_dir(uint8_t dir)   { _pin_dir(&DDRB, _BV(TP10), dir);   }
inline void mmr_tp10_set(uint8_t val)   { _pin_set(&PORTB, _BV(TP10), val);  }
inline uint8_t mmr_tp10_get(void)       { return _pin_get(&PINB, _BV(TP10)); }

inline void mmr_led_on(void)  { _pin_dir(&DDRD, _BV(LED1), OUTPUT); }
inline void mmr_led_off(void) { _pin_dir(&DDRD, _BV(LED1), INPUT);  }

inline void mmr_rdsint_mode(uint8_t mode) {  _pin_mode(&DDRD, _BV(RDSINT), mode); }
inline void mmr_rdsint_set(uint8_t val)   {  _pin_set(&PORTD, _BV(RDSINT), val);  }
inline uint8_t mmr_rdsint_get(void)       { return _pin_get(&PIND, _BV(RDSINT));  }

// Arduino-type delay
static inline void delay(uint16_t msec)
{
	_delay_ms(msec);
}

// in case if you managed to solder some wires to analogue inputs...
// Arduino-type analogRead()
static inline uint16_t analogRead(uint8_t channel)
{
	uint16_t val;
	// Select pin ADC0 using MUX
	ADMUX = _BV(REFS0) | (channel & 0x7); // VCC ref

	// Activate ADC with Prescaler 128 --> 8Mhz/128 = 62.5kHz
	ADCSRA =_BV(ADEN) |_BV(ADPS2) |_BV(ADPS1) | _BV(ADPS0);

	// Start conversion
	ADCSRA |= _BV(ADSC);

	// wait until conversion completed
	while(ADCSRA & _BV(ADSC));
	val = ADCW;
	ADCSRA &= ~_BV(ADEN);
	return val;
}

#ifdef __cplusplus
}
#endif
