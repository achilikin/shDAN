/* Simple command line parser for ATmega32

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
#ifndef SERIAL_CLI_H
#define SERIAL_CLI_H

#ifdef __cplusplus
extern "C" {
#if 0 // to trick VisualAssist
}
#endif
#endif

#ifndef CMD_LEN
#define CMD_LEN 0x7F // big enough for our needs
#endif

// command line processing, retuns:
//  0 - success
// -1 - invalid argument
// -2 - unknown command
typedef int8_t cli_processor(char *buf, void *ptr);

void   cli_init(void);
int8_t cli_interact(cli_processor *process, void *ptr);

// helper functions
char *get_arg(char *str);
const char *is_on(uint8_t val);

static inline int8_t str_is(const char *cmd, const char *str)
{
	return strcmp_P(cmd, str) == 0;
}

static inline uint16_t free_mem(void)
{
	extern int __heap_start, *__brkval; 
	unsigned val;
	val = (unsigned)((int)&val - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
	return val;
}

#ifdef __cplusplus
}
#endif
#endif
