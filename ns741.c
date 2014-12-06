/*
    FMBerry - an cheap and easy way of transmitting music with your Pi.
    https://github.com/Manawyrm/FMBerry
	
    Adapted for MMR-70 AVR Atmega32 by Andrey Chilikin https://github.com/achilikin

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
#include <stdio.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>

#include "rds.h"
#include "ns741.h"
#include "i2cmaster.h"

// I2C address of MMR-70
#define I2C_MMR70 ((0x66 << 1) | I2C_WRITE) // We do not read MMR-70

#define NS741_DEFAULT_F 9700 // default F=97.00MHz

// Calculates value of P for 0A/0B registers
#define NS741_FREQ(F) ((uint16_t)((uint32_t)F*10000ULL/8192ULL))

// Program Service name
char ps_name[8];
// Radiotext message
static uint8_t text_len = 64;
//                          "----------------------------64----------------------------------"
static char radiotext[64] = "MMR-70 as Temperature and Humidity sensor station               ";

uint8_t rds_register[4] = {0x03, 0x05, 0x05, 0x05};

static uint8_t group_index = 0; // group to be transmitted
static uint8_t block_index = 0; // block index within the group

// RDS 2A Group containing Radiotext chars
static uint16_t rds_text[4] =
{
	RDS_PI(RDS_RUSSIA,CAC_LOCAL,0),
	RDS_GT(2,0) | RDS_PTY(PTY_WEATHER),
	0, 0
};

// RDS 0B Group containing Program Service name
static uint16_t rds_ps[4] =
{
	RDS_PI(RDS_RUSSIA,CAC_LOCAL,0),
	RDS_GT(0,0) | RDS_PTY(PTY_WEATHER) | RDS_MS,
	0xE0CD, // E0 - No AF exists, CD - Filler code, see Table 11 of RBDS Standard
	0
};

#define RDS_MAX_GROUPS 20 // 4 groups - Program Service name, up to 16 - Radiotext
#define RDS_MAX_BLOCKS (RDS_MAX_GROUPS << 2)

// set to 0 to print groups
static uint8_t rds_debug = RDS_MAX_BLOCKS;
static uint8_t rds_debug_max = RDS_MAX_BLOCKS;

// I2C wrappers
// Send 8 bit
static void ns741_send_byte(uint8_t addr, uint8_t data)
{		
	i2c_start(I2C_MMR70);
	i2c_write(addr);
	i2c_write(data);
	i2c_stop();
}

// Send 16 bit, MSB first
static void ns741_send_word(uint8_t addr, uint8_t *data)
{
	i2c_start(I2C_MMR70);
	i2c_write(addr);
	i2c_write(data[1]);
	i2c_write(data[0]);
	i2c_stop();
}

// Send stream of bytes
static void ns741_send_data(uint8_t addr, const void *data, uint8_t len)
{
	const uint8_t *str = data;
	i2c_start(I2C_MMR70);
	i2c_write(addr);

	for(uint8_t i = 0; i < len; i++)
		i2c_write(str[i]);

	i2c_stop();
}

// NS741 functions

// register 0x00 controls power and oscillator:
//	bit 0 - power
//	bit 1 - oscillator
void ns741_radio(uint8_t on)
{
	uint8_t reg = 0x02; // oscillator is active
	if (on)
		reg |= 0x01; // power is active

	ns741_send_byte(0x00, reg);
	return;
}

// Initial register state, some change applied to 
// recommended values from TSMZ1-603 spec
static uint8_t init_data[] =
{
	// default map for all 22 RW registers
	0x02, // 00h: Power OFF, Crystal ON
	0xE1, // 01h: PE switch ON, PE selection 50us (Europe), Subcarrier ON, Pilot Level 1.6
	0x0A, // 02h: RFG 0.5 mW, Mute ON
	0x00, 0x00, 0x00, 0x00, // 03h-06h: RDS registers
	0x7E, 0x0E, // 07h-08h: recommended default
	0x08, // 09h: recommended default
	(NS741_FREQ(NS741_DEFAULT_F) & 0xFF), // 0Ah-0Bh, frequency
	((NS741_FREQ(NS741_DEFAULT_F) & 0xFF00) >> 8),
	0x0C, // 0Ch: recommended default
	0xE0, // 0Dh: ALC (Auto Level Control) OFF, AG (Audio Gain) -9dB
	0x30, // 0Eh: recommended default
	0x40, // 0Fh: Input Audio Gain -9dB
	0xA0, // 10h: RDS with checkword, RDS disabled
	0xE4, // 11h: recommended default
	0x00, // 12h: recommended default
	0x42, // 13h: recommended default
	0xC0, // 14h: recommended default
	0x41, // 15h: recommended default
	0xF4  // 16h: recommended default
};

int ns741_init(void)
{
	// make sure that I2C is initialized before calling ns741_init()
	// reset registers to default values
	ns741_send_data(0x00, init_data, sizeof(init_data));

	return 0;
}

// register 0x01 controls stereo subcarrier and pilot: 
//  bit 0: pre-emphasis on = 1
//  bit 1: pre-emphasis selection: 0 - 50us, 1 - 75us
//	bit 4: set to 1 to turn off subcarrier
//	bit 7: set to 0 to turn off pilot tone
// default: 0x81
void ns741_stereo(uint8_t on)
{
	uint8_t reg = init_data[1];
	if (on) {
		reg &= ~0x10; // enable subcarrier
		reg |= 0xE0;  // enable pilot tone at 1.6 level
	}
	else {
		reg |= 0x10;  // disable subcarrier
		reg &= ~0xE0; // disable pilot tone
	}
	
	init_data[1] = reg;
	ns741_send_byte(0x01, reg);
	return;
}

// register 0x02 controls output power and mute: 
//	RF output power 0-3 (bits 7-6) corresponding to 0.5, 0.8, 1.0 and 2.0 mW
//	reserved bits 1 & 3 are set
//	mute is off (bit 0)
// Set mute will turn off RDS as well as Pilot tone disabled
// default: 0x0A - 0.5mW + Mute ON
void ns741_mute(uint8_t on)
{
	uint8_t reg = init_data[2];
	if (on)
		reg |= 1;
	else
		reg &= ~1;

	ns741_send_byte(0x02, reg);
	init_data[2] = reg;
	return;
}

void ns741_txpwr(uint8_t strength)
{
	uint8_t reg = init_data[2];
	// clear RF power bits: set power level 0 - 0.5mW
	reg &= ~0xC0;
	strength &= 0x03; // just in case normalize strength
	reg |= (strength << 6);

	ns741_send_byte(0x02, reg);
	init_data[2] = reg;
	return;
}

// f_khz/10, for example 9500 for 95000KHz or 95MHz
void ns741_set_frequency(uint16_t f_khz)
{
	/* calculate frequency in 8.192kHz steps*/
	uint16_t val = NS741_FREQ(f_khz);

	// it is recommended to mute transmitter before changing frequency
	uint8_t reg = init_data[2];
	ns741_send_byte(0x02, reg | 0x01);

	ns741_send_byte(0x0A, val);
	ns741_send_byte(0x0B, val >> 8);

	// restore previous mute state
	ns741_send_byte(0x02, reg);
	return;
}

// Output gain 0-6, or -9dB to +9db, 3dB step
void ns741_volume(uint8_t gain)
{
	uint8_t reg = init_data[0x0D];
	if (gain > 6)
		gain = 6;
	reg &= ~0x0E;
	reg |= gain << 1;
	init_data[0x0D] = reg;

	ns741_send_byte(0x0D, reg);
}

// Muffle input gain -9dB on/off
void ns741_gain(uint8_t on)
{
	uint8_t reg = init_data[0x0F];

	if (on)
		reg |= 0x40;
	else
		reg &= ~0x40;
	init_data[0x0F] = reg;
	ns741_send_byte(0x0F, reg);
}

// register 0x10 controls RDS: 
//	  reserved bits 0-5, with bit 5 set
//	  bit 6: 0 - RDS off, 1 - RDS on
//    bit 7: RDS data format, 1 - with checkword
// default: 0xA0
void ns741_rds(uint8_t on)
{
	uint8_t reg = init_data[0x10];
	if (on)
		reg |= 0x40;
	else
		reg &= ~0x40;

	init_data[0x10] = reg;
	ns741_send_byte(0x10, reg);
	return;
}

// RDS_CP flag, third block type: C (cp=0) or C' (cp=1)
void ns741_rds_cp(uint8_t cp)
{		
	uint8_t reg = init_data[0x0F];
	if (cp)
		reg |=0x80;
	else
		reg &= 0x7F;
	init_data[0x0F] = reg;
	ns741_send_byte(0x0F, reg);
	return;
}

void ns741_rds_set_radiotext(const char *text)
{
	uint8_t i;

	// first copy text to our buffer
	for (i = 0; *text && (i < 64); i++, text++)
		radiotext[i] = *text;
	text_len = i;
	if (i < 64) {
		radiotext[i++] = '\r';
		// pad buffer with spaces
		for (; i < 64; i++)
			radiotext[i] = ' ';
	}
}

void ns741_rds_reset_radiotext(void)
{
	rds_text[1] ^= RDS_AB;
}

void ns741_rds_set_rds_pi(uint16_t rdspi)
{
	rds_ps[0] = rdspi;
	rds_ps[2] = rdspi;
	rds_text[0] = rdspi;
}

void ns741_rds_set_rds_pty(uint8_t rdspty)
{
	uint16_t pty = RDS_PTY(rdspty);
	rds_ps[1] &= ~RDS_PTYM;
	rds_ps[1] |= pty;
	rds_text[1] &= ~RDS_PTYM;
	rds_text[1] |= pty;
}

void ns741_rds_debug(uint8_t on)
{
	rds_debug = on ? 0x80 : RDS_MAX_BLOCKS;
}

// text - up to 8 characters
// shorter text will be padded with spaces for transmission
// so exactly 8 chars will be transmitted
void ns741_rds_set_progname(const char *text)
{
	uint8_t i;
	// first copy text to our buffer
	for (i = 0; *text && (i < 8); i++, text++)
		ps_name[i] = *text;
	// pad buffer with spaces
	for (; i < 8; i++)
		ps_name[i] = ' ';
}

// in total we can send 20 groups:
// 4 groups with Program Service Name and 16 with Radiotext
uint8_t ns741_rds_isr(void)
{
	uint8_t *data;
	static uint16_t *block;

	if (block_index == 0) {
		if (group_index > 3) {
			uint8_t i = (group_index - 4) << 2;
			if (i < text_len) {
				block = rds_text;
				block[1] &= ~RDS_RTIM;
				block[1] |= group_index - 4;
				data = (uint8_t *)&block[2];
				data[1] = radiotext[i];
				data[0] = radiotext[i+1];
				data[3] = radiotext[i+2];
				data[2] = radiotext[i+3];
			}
			else {
				rds_debug_max = group_index << 2;
				group_index = 0; // switch back to PS
			}
		}

		if (group_index < 4) {
			if ((group_index == 0) && (rds_debug & 0x80))
				rds_debug = 0;
			block = rds_ps;
			block[1] &= ~RDS_PSIM;
			block[1] |= group_index;
			data = (uint8_t *)&block[3];
			uint8_t i = group_index << 1; // 0,2,4,6
			data[1] = ps_name[i];
			data[0] = ps_name[i+1];
		}
		// uncomment following line if Group type B is used
		// ns741_rds_cp((block[1] & 0x0800) >> 8);
	}

	if (rds_debug < rds_debug_max) {
		data = (uint8_t *)&block[block_index];
		if (block_index == 0)
			printf_P(PSTR("%2d %02X%02X"), group_index, data[1], data[0]);
		else 
			printf_P(PSTR(" %02X%02X"), data[1], data[0]);
		if (block_index == 3)
			printf("\n");
		rds_debug++;
	}

	ns741_send_word(rds_register[block_index], (uint8_t *)&block[block_index]);
	block_index = (block_index + 1) & 0x03;
	if (!block_index)
		group_index = (group_index + 1) % RDS_MAX_GROUPS;
	return group_index;
}
