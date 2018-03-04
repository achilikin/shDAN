/* ILI9225 based display support for ATmega32L

   Copyright (c) 2018 Andrey Chilikin (https://github.com/achilikin)

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
#ifndef ILI9225_SPI_H
#define ILI9225_SPI_H

/**
	Limited set of functions for ILI9225 compatible LCD displays
	to minimize memory footprint if used on Atmel AVRs with low memory.
*/

#ifdef __cplusplus
extern "C" {
#if 0
} // dummy bracket for Visual Assist
#endif
#endif

// ili9225 screen size
#define ILI9225_MAX_X 175
#define ILI9225_MAX_Y 219
#define ILI9225_LCD_WIDTH  176
#define ILI9225_LCD_HEIGHT 220

// ili9225 registers
#define ILI9225_DRIVER_OUTPUT_CTRL       (0x01u)
#define ILI9225_DRIVER_OUTPUT_CTRL_SS    (0x01u << 8)
#define ILI9225_DRIVER_OUTPUT_CTRL_GS    (0x01u << 9)

#define ILI9225_LCD_AC_DRIVING_CTRL      (0x02u)

#define ILI9225_ENTRY_MODE               (0x03u)
// GRAM writing update direction
#define ILI9225_ENTRY_MODE_ADDR_VUPDATE  (0x01 << 3)
#define ILI9225_ENTRY_MODE_ADDR_HUPDATE  (0x00)
// Address counter increment/decrement direction
#define ILI9225_ENTRY_MODE_AC_VINCREMENT (0x01 << 5)
#define ILI9225_ENTRY_MODE_AC_VDECREMENT (0x00)
#define ILI9225_ENTRY_MODE_AC_HINCREMENT (0x01 << 4)
#define ILI9225_ENTRY_MODE_AC_HDECREMENT (0x00)

#define ILI9225_ENTRY_MODE_AM           (0x01u << 3)
#define ILI9225_DISP_CTRL1              (0x07u)
#define ILI9225_BLANK_PERIOD_CTRL1      (0x08u)
#define ILI9225_FRAME_CYCLE_CTRL        (0x0Bu)
#define ILI9225_INTERFACE_CTRL          (0x0Cu)
#define ILI9225_OSC_CTRL                (0x0Fu)
#define ILI9225_POWER_CTRL1             (0x10u)
#define ILI9225_POWER_CTRL2             (0x11u)
#define ILI9225_POWER_CTRL3             (0x12u)
#define ILI9225_POWER_CTRL4             (0x13u)
#define ILI9225_POWER_CTRL5             (0x14u)
#define ILI9225_VCI_RECYCLING           (0x15u)
#define ILI9225_RAM_HADDR               (0x20u) // horizontal GRAM address
#define ILI9225_RAM_VADDR               (0x21u) // vertical GRAM address
#define ILI9225_GRAM_DATA_REG           (0x22u)
#define ILI9225_GATE_SCAN_CTRL          (0x30u)
#define ILI9225_VERTICAL_SCROLL_CTRL1   (0x31u) // bottom line of a scroll area (end of scroll)
#define ILI9225_VERTICAL_SCROLL_CTRL2   (0x32u) // top line of a scroll area (start of scroll)
#define ILI9225_VERTICAL_SCROLL_CTRL3   (0x33u) // to how many lines to scroll
#define ILI9225_PARTIAL_DRIVING_POS1    (0x34u)
#define ILI9225_PARTIAL_DRIVING_POS2    (0x35u)
#define ILI9225_HORIZONTAL_WINDOW_ADDR1 (0x36u)
#define ILI9225_HORIZONTAL_WINDOW_ADDR2 (0x37u)
#define ILI9225_VERTICAL_WINDOW_ADDR1   (0x38u)
#define ILI9225_VERTICAL_WINDOW_ADDR2   (0x39u)
#define ILI9225_GAMMA_CTRL1             (0x50u)
#define ILI9225_GAMMA_CTRL2             (0x51u)
#define ILI9225_GAMMA_CTRL3             (0x52u)
#define ILI9225_GAMMA_CTRL4             (0x53u)
#define ILI9225_GAMMA_CTRL5             (0x54u)
#define ILI9225_GAMMA_CTRL6             (0x55u)
#define ILI9225_GAMMA_CTRL7             (0x56u)
#define ILI9225_GAMMA_CTRL8             (0x57u)
#define ILI9225_GAMMA_CTRL9             (0x58u)
#define ILI9225_GAMMA_CTRL10            (0x59u)

// converts RGB 24-bits RGB888 to 16-bits RGB565 color
#define RGB16(r,g,b) ((uint16_t)(((uint8_t)(r) & 0xf8) << 8) | (((uint8_t)(g) & 0xfc) << 3) | ((uint8_t)(b) >> 3))

// some predefined 16-bits colors
#define RGB16_BLACK          RGB16(0x00,0x00,0x00)
#define RGB16_WHITE          RGB16(0xFF,0xFF,0xFF)
#define RGB16_RED            RGB16(0xFF,0x00,0x00)
#define RGB16_GREEN          RGB16(0x00,0xFF,0x00)
#define RGB16_BLUE           RGB16(0x00,0x00,0xFF)
#define RGB16_YELLOW         RGB16(0xFF,0xFF,0x00)
#define RGB16_CYAN           RGB16(0x00,0xFF,0xFF)
#define RGB16_MAGENTA        RGB16(0xFF,0x00,0xFF)
#define RGB16_DARKRED        RGB16(0x80,0x00,0x00)
#define RGB16_DARKBLUE       RGB16(0x00,0x00,0x8B)
#define RGB16_DARKGREEN      RGB16(0x00,0x64,0x00)
#define RGB16_DARKCYAN       RGB16(0x00,0x8B,0x8B)
#define RGB16_NAVY           RGB16(0x00,0x00,0x80)
#define RGB16_OLIVE          RGB16(0x80,0x80,0x00)
#define RGB16_GRAY           RGB16(0x80,0x80,0x80)
#define RGB16_DARKGRAY       RGB16(0xA9,0xA9,0xA9)
#define RGB16_LIGHTGRAY      RGB16(0xD3,0xD3,0xD3)
#define RGB16_LIGHTGREEN     RGB16(0x90,0xEE,0x90)
#define RGB16_LIGHTBLUE      RGB16(0xAD,0xD8,0xE6)
#define RGB16_LIGHTCYAN      RGB16(0xE0,0xFF,0xFF)
#define RGB16_SILVER         RGB16(0xC0,0xC0,0xC0)
#define RGB16_GOLD           RGB16(0xFF,0xD7,0x00)
#define RGB16_ORANGE         RGB16(0xFF,0xA5,0x00)

#define ILI_LED_PIN    0x01
#define ILI_LED_PWM    0x02
#define ILI_LED_OFF    0x04
#define ILI_DISP_OFF   0x08
#define ILI_DISP_SLEEP 0x10

// ili9225 control structure
typedef struct ili9225_s
{
	uint8_t  flags; // ILI_LED_*/ILI_DISP_* flags above
	uint8_t  cs;    // CS pin, use PN* defines from pinio.h
	uint8_t  rs;    // RS pin, use PN* defines from pinio.h
	uint8_t  rst;   // RST pin, use PN* defines from pinio.h
	uint8_t  led;   // LED pin, use PN* defines from pinio.h
	uint16_t fcolor;
	uint16_t bcolor;
} ili9225_t;

// screen initialization
void ili9225_init(ili9225_t *ili);

// switch lcd back light on or off
#define ILI9225_BKL_OFF 0
#define ILI9225_BKL_ON  0xFF

// mode: ILI9225_BKL_OFF/ILI9225_BKL_ON or any value 0-255 range
//       if connected to PWM pin
void ili9225_set_backlight(ili9225_t *ili, uint8_t mode); 

// switch display (including back light) off, on or put to standby
#define ILI9225_DISP_OFF     0 // ~1mA
#define ILI9225_DISP_ON      1 // ~3mA + ~25mA back light
#define ILI9225_DISP_STANDBY 2 // ~12uA
void ili9225_set_disp(ili9225_t *ili, uint8_t mode);

// set foreground color, will affect all drawing functions below
static inline void ili9225_set_fg_color(ili9225_t *ili, uint16_t color) {
	ili->fcolor = color;
}

// set background color, will affect ili9225_clear() and ili9225_puts()
static inline void ili9225_set_bk_color(ili9225_t *ili, uint16_t color) {
	ili->bcolor = color;
}

// swap foreground and background colors
void ili9225_swap_color(ili9225_t *ili);

// clear the screen
void ili9225_clear(ili9225_t *ili); 

// set pixel color
void ili9225_set_pixel(ili9225_t *ili, uint8_t x, uint8_t y);

// draw a rectangle
void ili9225_rectangle(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

// fill a solid rectangle
void ili9225_fill(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

// draw a line
void ili9225_line(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

// draw text string using font selected by bmfont_select() function
void ili9225_text(ili9225_t *ili, uint8_t x, uint8_t y, const char *str, uint8_t atr);

// set scroll area
void ili9225_set_scroll(ili9225_t *ili, uint8_t start, uint8_t end);

// scroll selected area
void ili9225_scroll(ili9225_t *ili, uint8_t lines);

// set display direction
#define ILI9225_DISP_NORMAL 0 // ILI925 chip at the bottom
#define ILI9225_DISP_UPDOWN 1 // ILI925 chip at the top

void ili9225_set_dir(ili9225_t *ili, uint8_t dir);

#endif

