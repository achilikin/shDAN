/* Simple command line parser for ATmega32 on MMR-70

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
#ifndef __COMMAND_LINE_H__
#define __COMMAND_LINE_H__

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

#define CMD_LEN 0x07F // big enough to accommodate RDS text

void   cli_init(void);
int8_t cli_interact(void *rht);

#ifdef __cplusplus
}
#endif
#endif
