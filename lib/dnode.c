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
#include <stdint.h>
#include <string.h>

#include "dnode.h"
#include "i2cmem.h"

uint8_t ts_unpack(dnode_t *tsync)
{
	uint8_t nid = tsync->raw[0] >> 4;
	// unpack hour
	uint8_t ts = tsync->raw[1] >> 4;
	ts |= (tsync->raw[0] & 0x01) << 4;
	tsync->raw[0] = ts;
	// unpack minute
	ts = (tsync->raw[1] & 0x0F) << 2;
	ts |= (tsync->raw[2] >> 6);
	tsync->raw[1] = ts;
	// unpack seconds
	tsync->raw[2] &= 0x3F;
	return nid;
}

void ts_pack(dnode_t *tsync, uint8_t nid)
{
	// pack minutes
	uint8_t ts = tsync->raw[1];
	tsync->raw[2] |= ts << 6;
	tsync->raw[1] >>= 2;
	// pack hour
	ts = tsync->raw[0];
	tsync->raw[1] |= ts << 4;
	tsync->raw[0] = ts >> 4;
	// add node id as destination
	tsync->raw[0] |= nid << 4;
}

static const uint16_t LOG_RECNUM = 24 * 60;
static const uint16_t LOG_SIZE = (24 * 60 * sizeof(dnode_log_t));

void log_erase(uint8_t lidx)
{
	uint8_t page[I2C_MEM_PAGE_SIZE];
	memset(page, 0, I2C_MEM_PAGE_SIZE);

	uint16_t wr = 0, log_addr = LOG_SIZE * lidx;
	uint16_t n = LOG_SIZE / I2C_MEM_PAGE_SIZE;
	for(uint16_t i = 0; i < n; i++) {
		i2cmem_write_data(log_addr + wr, page, I2C_MEM_PAGE_SIZE);
		wr += I2C_MEM_PAGE_SIZE;
	}
	if (wr < LOG_SIZE)
		i2cmem_write_data(log_addr + wr, page, LOG_SIZE - wr);
}

uint16_t log_next_rec_index(uint16_t ridx)
{
	uint16_t next = ridx + 1;
	if (next < LOG_RECNUM)
		return next;
	return 0;
}

int8_t log_read_rec(uint8_t lidx, uint16_t ridx, dnode_log_t *rec)
{
	uint16_t addr = LOG_SIZE * lidx + ridx*sizeof(dnode_log_t);
	return i2cmem_read_data(addr, rec, sizeof(dnode_log_t));
}

int8_t log_write_rec(uint8_t lidx, uint16_t ridx, dnode_log_t *rec)
{
	uint16_t addr = LOG_SIZE * lidx + ridx*sizeof(dnode_log_t);
	return i2cmem_write_data(addr, rec, sizeof(dnode_log_t));
}

int8_t log_erase_rec(uint8_t lidx, uint16_t ridx)
{
	dnode_log_t rec;
	rec.ssi = 0;
	rec.data.v16 = 0u;
	return log_write_rec(lidx, ridx, &rec);
}
