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

#include "dnode.h"

uint8_t ts_unpack(dnode_t *tsync)
{
	uint8_t nid = tsync->data[0] >> 4;
	// unpack hour
	uint8_t ts = tsync->data[1] >> 4;
	ts |= (tsync->data[0] & 0x01) << 4;
	tsync->data[0] = ts;
	// unpack minute
	ts = (tsync->data[1] & 0x0F) << 2;
	ts |= (tsync->data[2] >> 6);
	tsync->data[1] = ts;
	// unpack seconds
	tsync->data[2] &= 0x3F;
	return nid;
}

void ts_pack(dnode_t *tsync, uint8_t nid)
{
	// pack minutes
	uint8_t ts = tsync->data[1];
	tsync->data[2] |= ts << 6;
	tsync->data[1] >>= 2;
	// pack hour
	ts = tsync->data[0];
	tsync->data[1] |= ts << 4;
	tsync->data[0] = ts >> 4;
	// add node id as destination
	tsync->data[0] |= nid << 4;
}
