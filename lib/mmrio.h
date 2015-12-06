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
#ifndef MMR70_PIN_IO_H
#define MMR70_PIN_IO_H

#include <avr/io.h>
#include <util/delay.h>

#include "pinio.h"

// MMR-70 has only some pins of ports B and D populated
#define TP4		PB0 // TP4, T0
#define TP5		PB1 // TP5, T1
#define EXSS    PB4 // SPI SS, if soldered 
#define TP7     PB5	// TP7, MOSI
#define TP3     PB6	// TP3, MISO
#define TP10    PB7	// TP10, SCK

#define MMR_RXD PD0
#define MMR_TXD PD1

#define MMR_RDSINT	PD2 // RDSINT
#define MMR_LED1	PD7 // LED1

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

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

#ifdef __cplusplus
}
#endif
#endif
