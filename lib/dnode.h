/*  Copyright (c) 2015 Andrey Chilikin (https://github.com/achilikin)
    
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
#ifndef DATA_NODE_H
#define DATA_NODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

#define NID_MASK   0x0F // node index mask
#define NODE_TSYNC 0x80 // time sync request
#define SENS_MASK  0x70 // sensor index mask

#define SET_NID(nid,sens) (((sens << 4) & SENS_MASK) | (nid & NID_MASK))

// reserved sensor indexes
#define SENS_TXPWR 0x00 // radio TX power
#define SENS_LIST  0x70 // list of sensors in data[]

// up to 15 different sensor types
#define SENS_TEMPER 0x01 // temperature, i8 val + u8 dec
#define SENS_HUMID  0x02 // humidity, u8 val + u8 dec
#define SENS_PRESS  0x03 // pressure, u16 bit value
#define SENS_LIGHT  0x04 // light, 10 bit adc
#define SENS_DIGIT  0x05 // digital, 16bit mask
#define SENS_COUNT  0x06 // counter, u16 value

#define STAT_SLEEP 0x80 // node is in sleep mode
#define STAT_LED   0x40 // poll led is on
#define STAT_VBAT  0x0F // Vbat mask

/*
 nid bits: tsssnnnn
 t:    time sync request/reply
 sss:  sensor id, 0 - RFM TX power, 7 - sensors description
 nnnn: node id, 0-base station, 1-11 nodes for 1 minute multiplexing cycle
 sss/nnnn combination is used to build simple time-division multiplexing,
 for example if there are no more than 4 different sensors (zones) per
 remote node, then up to 11 nodes can share one minute for transmitting data
 Start/stop of a transmission is calculated as
   start = (node - 1)*5
 so node 1 transmit first zone data at 00 sec of every minute,
 node 2 at 05 sec of every minute and so one

 stat bits: sl00vvvv
 s: sleep mode is on
 l: led indication is on
 vvvv: Vbat=2.30 + vvvv*0.1
*/
typedef struct dnode_s
{
	uint8_t nid;
	union {
		struct {
			int8_t  stat; // Vbat = ((stat & STAT_VBAT)*10 + 230)/100.0
			union {
				struct {
					int8_t  val; // value
					uint8_t dec; // decimal
				};
				uint16_t v16; // for example: air pressure, hP
			};

		};
		struct {
			uint8_t hour;
			uint8_t min;
			uint8_t sec;
		};
		uint8_t data[3];
	};
} dnode_t;

#ifdef __cplusplus
}
#endif

#endif
