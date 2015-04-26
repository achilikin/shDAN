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

#define MAX_DNODES  12
#define MAX_SENSORS 6

#define NID_MASK   0x0F // node index mask
#define NODE_TSYNC 0x80 // time sync request
#define SENS_MASK  0x70 // sensor index mask

#define SET_NID(nid,sens) ((((sens) << 4) & SENS_MASK) | ((nid) & NID_MASK))
#define GET_NID(nid) ((nid) & NID_MASK)

#define SET_SENS(sens,type) (((type) << 4) | ((sens) & 0x0F))
#define GET_SENS(nid) (((nid) & SENS_MASK) >> 4)

// reserved sensor indexes
#define SENS_TXPWR 0x00 // radio TX power
#define SENS_LIST  0x70 // list of sensors in data[]

// up to 15 different sensor types (0x01-0x0F)
#define SENS_TEMPER 0x01 // temperature, i8 val + u8 dec
#define SENS_HUMID  0x02 // humidity, u8 val + u8 dec
#define SENS_PRESS  0x03 // pressure, u16 bit value
#define SENS_LIGHT  0x04 // light, 10 bit adc
#define SENS_DIGIT  0x05 // digital, 16bit mask
#define SENS_COUNT  0x06 // counter, u16 value

#define STAT_SLEEP 0x80 // node is in sleep mode
#define STAT_LED   0x40 // poll led is on
#define STAT_ACK   0x20 // cmd acknowledgment
#define STAT_EOS   0x10 // end of session (last message)
#define STAT_VBAT  0x0F // Vbat mask

// extra flags for dnode_status
#define STAT_ACTIVE 0x01
#define STAT_SLIST  0x02
#define STAT_TSYNC  0x04

// command to be applied for .nid sensor id
#define CMD_GVAL   0x10 // get value
#define CMD_SVAL   0x20 // set value
#define CMD_GBIT   0x30 // get bit, cval[0] = bit index
#define CMD_SBIT   0x40 // set bit, cval[0] = bit index, cval[1] - bit value
#define CMD_SNODE  0x50 // set node state, cval[0] - 0: always active, 1: sleep
#define CMD_SLED   0x60 // set LED state, cval[0] - 0/1

/*
 nid bits: tsssnnnn
 t:   time sync request/reply
 sss: sensor id, 0 - RF TX power, 7 - sensors description, 1-6 attached sensors
 nnnn: node id, 0-base station, 1-12 data nodes, 13-15 reserved
       node id used to build simple time-division multiplexing schema,
       start of node's transmission can be calculated as
          start = (node - 1)*5
       so node 1 transmit first message at 00 sec of every minute,
       node 2 at 05 sec of every minute and so one.
	   Session cannot be longer that 5 seconds, last message should have EOS
	   bit set to indicate End of Session, so base station can sent messages
	   to AA (Always Active) nodes or other nodes can transmit urgent data

 stat bits: sla0vvvv
 s: sleep mode is on
 l: led indication is on
 a: command acknowledgment
 e: end of session, no more message till next session
 vvvv: Vbat=2.30 + vvvv*0.1

 cmd bits: ddddcccc
 d: destination node id
 c: command
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
		struct {
			int8_t  cmd; // ddddcccc
			union {
				uint8_t cval[2]; // command value
				uint16_t cval16; // 16bit command value
			};

		};
		uint8_t data[3];
	};
} dnode_t;

typedef int8_t sens_poll(dnode_t *dval, void *ptr);

typedef struct dnode_status_s {
	uint8_t flags;
	uint8_t ts[3]; // last session time stamp
	uint8_t vbat;
	uint8_t ssi; // signal strength indicator 0-100%
}dnode_status_t;

typedef struct dsens_s
{
	uint8_t tos_sid; // ToS: 4 MSB, SID: 4 LSB
	sens_poll *poll;
	void *data;
} dsens_t;

static inline int8_t set_sens_type(dnode_t *dval, uint8_t sid, uint8_t type)
{
	if (!sid || sid > 6 || type > 16)
		return -1;
	int8_t idx = (sid - 1) / 2;
	if (!(sid & 0x01))
		type <<= 4;
	dval->data[idx] |= type;
	return 0;
}

static inline uint8_t get_sens_type(dnode_t *dval, uint8_t sid)
{
	if (!sid || sid > 6)
		return -1;
	int8_t idx = (sid - 1) / 2;
	uint8_t type = dval->data[idx];
	if (!(sid & 0x01))
		type >>= 4;
	return type;
}

uint8_t ts_unpack(dnode_t *tsync);
void ts_pack(dnode_t *tsync, uint8_t nid);

#ifdef __cplusplus
}
#endif

#endif
