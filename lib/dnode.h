/*  Copyright (c) 2018 Andrey Chilikin (https://github.com/achilikin)
    
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

#define MAX_SENSORS    6
#define MAX_DNODE_LOGS 5
#define MAX_DNODE_NUM  12

#define NODE_LBS       13 // listening base station

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
// we can use 6 lower bits as only STAT_SLEEP and STAT_LED are relevant
#define DANF_MASK   (STAT_ACK | STAT_EOS | STAT_VBAT)
#define DANF_VALID  0x10
#define DANF_ACTIVE 0x01
#define DANF_SLIST  0x02
#define DANF_TSYNC  0x04
#define DANF_LOG    0x08

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

typedef union dsens_data_u {
	struct {
		uint8_t val; // value, high bit: negative value
		uint8_t dec; // decimal
	};
	uint16_t v16; // for example: air pressure, hP
} dsens_data_t;

static inline int8_t get_dval(uint8_t val) {
	if (val & 0x80)
		return -(val & 0x7F);
	return val;
}

typedef struct dnode_s
{
	uint8_t nid;
	union {
		struct {
			int8_t  stat; // Vbat = ((stat & STAT_VBAT)*10 + 230)/100.0
			dsens_data_t data;
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
		uint8_t raw[3];
	};
} dnode_t;


#define NODE_NAME_LEN 6

typedef struct dnode_status_s {
	uint8_t flags;
	uint8_t nid;
	uint8_t ts[3]; // last session time stamp
	uint8_t tout;  // timeout counter
	uint8_t vbat;
	uint8_t ssi; // signal strength indicator 0-100%
	uint8_t log;
	uint8_t stype[6]; // sensor types
	dsens_data_t sdata[6]; // sensor data
	uint8_t name[NODE_NAME_LEN];
} dnode_status_t;

typedef int8_t sens_poll(dnode_t *dval, void *ptr);

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
	dval->raw[idx] |= type;
	return 0;
}

static inline uint8_t get_sens_type(dnode_t *dval, uint8_t sid)
{
	if (!sid || sid > 6)
		return -1;
	int8_t idx = (sid - 1) / 2;
	uint8_t type = dval->raw[idx];
	if (!(sid & 0x01))
		type >>= 4;
	return type;
}

uint8_t ts_unpack(dnode_t *tsync);
void ts_pack(dnode_t *tsync, uint8_t nid);

typedef struct dnode_log_s {
	uint8_t      ssi;
	dsens_data_t data;
} dnode_log_t;

// logging functions
void   log_erase(uint8_t lidx);
uint16_t log_next_rec_index(uint16_t ridx);
int8_t log_erase_rec(uint8_t lidx, uint16_t ridx);
int8_t log_write_rec(uint8_t lidx, uint16_t ridx, dnode_log_t *rec);
int8_t log_read_rec(uint8_t lidx, uint16_t ridx, dnode_log_t *rec);

#ifdef __cplusplus
}
#endif

#endif
