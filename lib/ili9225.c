#include <stdlib.h>
#include <avr/io.h>
#define __DELAY_BACKWARD_COMPATIBLE__
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "spi.h"
#include "timer.h"
#include "bmfont.h"
#include "ili9225.h"

#define ILI_WRCMD  0
#define ILI_WRDATA 1

// two panel types from ILI9225 app notes
#define PANEL_HYDIS 0
#define PANEL_CMO   1
#define ILI_PANEL PANEL_HYDIS

// local static functions
static inline void ili_write_mode(ili9225_t *ili, uint8_t data)
{
	digitalWrite(ili->rs, data);
}

static inline void ili_spi_select(ili9225_t *ili)
{
	digitalWrite(ili->cs, LOW);
}

static inline void ili_spi_unselect(ili9225_t *ili)
{
	digitalWrite(ili->cs, HIGH);
}

static void ili_write_cmd(ili9225_t *ili, uint16_t cmd)
{
	ili_spi_select(ili);
	ili_write_mode(ili, ILI_WRCMD);
	spi_write_word(cmd);
	ili_spi_unselect(ili);
}

static void ili_write_reg(ili9225_t *ili, uint16_t reg, uint16_t data)
{
	ili_spi_select(ili);
	ili_write_mode(ili, ILI_WRCMD);
	spi_write_word(reg);
	ili_write_mode(ili, ILI_WRDATA);
	spi_write_word(data);
	ili_spi_unselect(ili);
}

static void ili_set_wndow(ili9225_t *ili, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	ili_write_reg(ili, ILI9225_HORIZONTAL_WINDOW_ADDR1, x1);
	ili_write_reg(ili, ILI9225_HORIZONTAL_WINDOW_ADDR2, x0);

	ili_write_reg(ili, ILI9225_VERTICAL_WINDOW_ADDR1, y1);
	ili_write_reg(ili, ILI9225_VERTICAL_WINDOW_ADDR2, y0);

	ili_write_reg(ili, ILI9225_RAM_HADDR, x0);
	ili_write_reg(ili, ILI9225_RAM_VADDR, y0);

	ili_write_cmd(ili, ILI9225_GRAM_DATA_REG);
}

// exported functions
void ili9225_init(ili9225_t *ili)
{
	// Set up pins
	pinMode(ili->rs, OUTPUT);
	pinMode(ili->cs, OUTPUT_HIGH);
	pinMode(ili->rst, OUTPUT_HIGH);
	if (ili->flags & ILI_LED_PIN)
		pinMode(ili->led, OUTPUT);

	ili9225_set_backlight(ili, ILI9225_BKL_OFF);

	// hard reset sequence
	digitalWrite(ili->rst, 1);
	delay(1); 
	digitalWrite(ili->rst, 0); // Pull the reset pin low to reset ILI9225
	delay(10);
	digitalWrite(ili->rst, 1); // Pull the reset pin high to release the ILI9225C from the reset status
	delay(50);

	// initialization sequence
	ili_write_reg(ili, ILI9225_DISP_CTRL1, 0x0000u); // display off
	ili_write_reg(ili, ILI9225_DRIVER_OUTPUT_CTRL, 0x011Cu);
	ili_write_reg(ili, ILI9225_LCD_AC_DRIVING_CTRL, 0x0100u);
	ili_write_reg(ili, ILI9225_ENTRY_MODE, 0x1030u); // BRG and ID1/0
	ili_write_reg(ili, ILI9225_BLANK_PERIOD_CTRL1, 0x0808u);
	ili_write_reg(ili, ILI9225_INTERFACE_CTRL, 0x0001u); // RGB 16bit
	ili_write_reg(ili, ILI9225_OSC_CTRL, 0x0801u);
	ili_write_reg(ili, ILI9225_RAM_HADDR, 0x0000u); // RAM horizontal address
	ili_write_reg(ili, ILI9225_RAM_VADDR, 0x0000u); // RAM vertical address

	// Power-on sequence
	ili_write_reg(ili, ILI9225_POWER_CTRL1, 0x0A00u);
	ili_write_reg(ili, ILI9225_POWER_CTRL2, 0x103Bu);
	delay(50);
#if (ILI_PANEL == PANEL_HYDIS)
	ili_write_reg(ili, ILI9225_POWER_CTRL3, 0x6121u);
	ili_write_reg(ili, ILI9225_POWER_CTRL4, 0x006Fu);
	ili_write_reg(ili, ILI9225_POWER_CTRL5, 0x495Fu);
#else
	ili_write_reg(ili, ILI9225_POWER_CTRL3, 0x3121u);
	ili_write_reg(ili, ILI9225_POWER_CTRL4, 0x0066u);
	ili_write_reg(ili, ILI9225_POWER_CTRL5, 0x3660u);
#endif
	ili_write_reg(ili, ILI9225_FRAME_CYCLE_CTRL, 0x1100u);
	ili_write_reg(ili, ILI9225_VCI_RECYCLING, 0x0020u); // Set VCI recycling

	// set GRAM area
	ili_write_reg(ili, ILI9225_GATE_SCAN_CTRL, 0x0000u); 
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL1, 0x00DBu); 
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL2, 0x0000u); 
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL3, 0x0000u); 
	ili_write_reg(ili, ILI9225_PARTIAL_DRIVING_POS1, 0x00DBu); 
	ili_write_reg(ili, ILI9225_PARTIAL_DRIVING_POS2, 0x0000u); 
	ili_write_reg(ili, ILI9225_HORIZONTAL_WINDOW_ADDR1, 0x00AFu); 
	ili_write_reg(ili, ILI9225_HORIZONTAL_WINDOW_ADDR2, 0x0000u); 
	ili_write_reg(ili, ILI9225_VERTICAL_WINDOW_ADDR1, 0x00DBu); 
	ili_write_reg(ili, ILI9225_VERTICAL_WINDOW_ADDR2, 0x0000u); 

	// set GAMMA curve
#if (ILI_PANEL == PANEL_HYDIS)
	ili_write_reg(ili, ILI9225_GAMMA_CTRL1, 0x0000u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL2, 0x0808u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL3, 0x080Au);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL4, 0x000Au);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL5, 0x0A08u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL6, 0x0808u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL7, 0x0000u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL8, 0x0A00u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL9, 0x1007u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL10, 0x0710u);
#else
	ili_write_reg(ili, ILI9225_GAMMA_CTRL1, 0x0400u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL2, 0x080Bu);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL3, 0x0E0Cu);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL4, 0x0103u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL5, 0x0C0Eu);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL6, 0x0B08u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL7, 0x0004u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL8, 0x0301u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL9, 0x0E00u);
	ili_write_reg(ili, ILI9225_GAMMA_CTRL10, 0x000Eu);
#endif

	delay(50); 
	ili_write_reg(ili, ILI9225_DISP_CTRL1, 0x1017u);

	ili9225_set_fg_color(ili, RGB16_WHITE);
	ili9225_set_bk_color(ili, RGB16_BLACK);
	ili9225_clear(ili);
	ili9225_set_backlight(ili, ILI9225_BKL_ON);
}

void ili9225_set_backlight(ili9225_t *ili, uint8_t mode)
{
	if (ili->flags & ILI_LED_PIN) {
		if (ili->flags & ILI_LED_PWM)
			pwm_set_duty(mode);
		else
			digitalWrite(ili->led, mode);
	}
}

uint16_t ili9225_set_fg_color(ili9225_t *ili, uint16_t color)
{
	uint16_t fcolor = ili->fcolor;
	ili->fcolor = color;
	return fcolor;
}

void ili9225_set_bk_color(ili9225_t *ili, uint16_t color)
{
	ili->bcolor = color;
}

void ili9225_swap_color(ili9225_t *ili)
{
	uint16_t color = ili->bcolor;
	ili->bcolor = ili->fcolor;
	ili->fcolor = color;
}

void ili9225_set_disp(ili9225_t *ili, uint8_t mode)
{
	switch(mode) {
	case ILI9225_DISP_OFF:
	case ILI9225_DISP_STANDBY:
		ili9225_set_backlight(ili, ILI9225_BKL_OFF);
		ili_write_reg(ili, ILI9225_DISP_CTRL1, 0x0000u);
		if (mode == ILI9225_DISP_OFF)
			return;
		delay(50);
		ili_write_reg(ili, ILI9225_POWER_CTRL2, 0x0007u);
		delay(50);
		ili_write_reg(ili, ILI9225_POWER_CTRL1, 0x0A01u);
		break;
	default:
		ili_write_reg(ili, ILI9225_POWER_CTRL1, 0x0A00u);
		ili_write_reg(ili, ILI9225_POWER_CTRL2, 0x1038u);
		delay(50);
		ili_write_reg(ili, ILI9225_DISP_CTRL1, 0x1017u);
		ili9225_set_backlight(ili, ILI9225_BKL_ON);
		break;
	}
}

void ili9225_fill(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	ili_set_wndow(ili, x1, y1, x2, y2);
	ili_spi_select(ili);
	ili_write_mode(ili, ILI_WRDATA);
	uint8_t y = y2 - y1 + 1;
	uint8_t x = x2 - x1 + 1;
	uint16_t color = ili->fcolor;
	for(; y != 0; y--) {
		for(uint8_t i = x; i != 0; i--)
			spi_write_word(color);
	}
	ili_spi_unselect(ili);
	ili_set_wndow(ili, 0, 0, ILI9225_LCD_WIDTH, ILI9225_LCD_HEIGHT);
}

void ili9225_clear(ili9225_t *ili)
{
	uint16_t color = ili->fcolor;
	ili->fcolor = ili->bcolor;
	ili9225_fill(ili, 0, 0, ILI9225_LCD_WIDTH - 1, ILI9225_LCD_HEIGHT - 1);
	ili->fcolor = color;
}

void ili9225_rectangle(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	ili9225_fill(ili, x1, y1, x2, y1);
	ili9225_fill(ili, x1, y2, x2, y2);
	ili9225_fill(ili, x1, y1, x1, y2);
	ili9225_fill(ili, x2, y1, x2, y2);
}

void ili9225_goto(ili9225_t *ili, uint8_t x, uint8_t y)
{
	ili_write_reg(ili, ILI9225_RAM_HADDR, x);
	ili_write_reg(ili, ILI9225_RAM_VADDR, y);
}

void ili9225_set_pixel(ili9225_t *ili, uint8_t x, uint8_t y)
{
	ili9225_goto(ili, x, y);

	ili_spi_select(ili);
	ili_write_mode(ili, ILI_WRCMD);
	spi_write_word(ILI9225_GRAM_DATA_REG);
	ili_write_mode(ili, ILI_WRDATA);
	spi_write_word(ili->fcolor);
	ili_spi_unselect(ili);
}

void ili9225_text(ili9225_t *ili, uint8_t x, uint8_t y, const char *str, uint8_t atr)
{
	bmfont_t *pfont = bmfont_get();
	uint8_t gw = pfont->gw;
	uint8_t gh = pfont->gh;
	uint8_t go = pfont->go;
	uint8_t gb = gw*(gh / 8); // bytes per glyph
	uint8_t glines = gh / 8;  // lines per glyph
	const uint8_t *font = pfont->font;
	uint16_t color[2];
	color[0] = ili->bcolor;
	color[1] = ili->fcolor;

	uint8_t rev = 0;
	if (atr & TEXT_REVERSE) rev = ~rev;
	uint8_t over = 0;
	if (atr & TEXT_OVERLINE)
		over = 0x01;
	uint8_t under = 0;
	if (atr & TEXT_UNDERLINE)
		under = 0x80;

	for(; *str != '\0'; str++, x += gw) {
		uint16_t idx = (*str - go) * gb;
		for(uint8_t l = 0; l < glines; l++) {
			for(uint8_t n = 0; n < gw; n++) {
				uint8_t d = pgm_read_byte(&font[idx+n +l*gw]);
				d ^= rev;
				if (l == (glines - 1))
					d ^= under;
				if (l == 0)
					d ^= over;
				ili_set_wndow(ili, x+n, y+8*l, x+n, y+8+8*l);
				ili_spi_select(ili);
				ili_write_mode(ili, ILI_WRDATA);
				for(uint8_t i = 0; i < 8; i++) {
					spi_write_word(color[d & 0x01]);
					d >>= 1;
				}
			}
		}
	}

	ili_set_wndow(ili, 0, 0, ILI9225_LCD_WIDTH, ILI9225_LCD_HEIGHT);
}

static inline void swap_u8(uint8_t *a, uint8_t *b)
{
	uint8_t tmp = *a;
	*a = *b;
	*b = tmp;
}

void ili9225_line(ili9225_t *ili, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	if ((x1 == x2) || (y1 == y2)) {
		ili9225_fill(ili, x1, y1, x2, y2);
		return;
	}

	// Bresenham line algorithm
	uint8_t steep = abs((int16_t)(y2 - y1)) > abs((int16_t)(x2 - x1));
	uint8_t dx;
	int16_t dy;

	if (steep) {
		swap_u8(&x1, &y1);
		swap_u8(&x2, &y2);
	}

	if (x1 > x2) {
		swap_u8(&x1, &x2);
		swap_u8(&y1, &y2);
	}

	dx = x2 - x1;
	dy = abs(y2 - y1);

	int16_t err = dx / 2;
	int8_t ystep = (y1 < y2) ? 1 : -1;

	for (; x1 <= x2; x1++) {
		if (steep)
			ili9225_set_pixel(ili, y1, x1);
		else
			ili9225_set_pixel(ili, x1, y1);

		err -= dy;
		if (err < 0) {
			y1 += ystep;
			err += dx;
		}
	}
}

void ili9225_set_scroll(ili9225_t *ili, uint8_t start, uint8_t end)
{
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL2, start);
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL1, end);
}

void ili9225_scroll(ili9225_t *ili, uint8_t lines)
{
	ili_write_reg(ili, ILI9225_VERTICAL_SCROLL_CTRL3, lines);
}

static inline void ili9225_set_entry_mode(ili9225_t *ili, uint8_t mode)
{
	ili_write_reg(ili, ILI9225_ENTRY_MODE, 0x1000 | (mode << 3));
}

static inline void ili9225_set_shift(ili9225_t *ili, uint8_t shift)
{
	ili_write_reg(ili, ILI9225_DRIVER_OUTPUT_CTRL, 0x001Cu | (shift << 8));
}

void ili9225_set_dir(ili9225_t *ili, uint8_t dir)
{
	uint16_t octl = 0x001C;
	if (dir == ILI9225_DISP_UPDOWN)
		octl |= ILI9225_DRIVER_OUTPUT_CTRL_GS;
	else
		octl |= ILI9225_DRIVER_OUTPUT_CTRL_SS;

	ili_write_reg(ili, ILI9225_DRIVER_OUTPUT_CTRL, octl);
}
