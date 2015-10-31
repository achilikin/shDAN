/* Si4420 (HopeRF RFM12BS) transiver support for ATmega32L on MMR-70
   http://www.silabs.com/products/wireless/EZRadio/Pages/Si442021.aspx
   http://www.hoperf.com/rf/fsk_module/RFM12B.htm

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
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

#include "uart.h"

#include "spi.h"
#include "pinio.h"
#include "timer.h"
#include "rfm12bs.h"

#define VBAT_TIMEOUT 5 // Vbat interrupt timeout, msec

static uint8_t rfm_timeout = 50; // default TX timeout
static uint8_t rfm_txctl;
static uint8_t rfm_rxctl;
static uint8_t rfm_band;
static uint8_t rfm_sync;

// waits timeout msecs for nIRQ pin to go to LOW state
// returns -1 if timed out, or msec spent in the wait loop
static int8_t rfm_wait_irq(rfm12_t *rfm, uint8_t timeout)
{
	uint8_t dt, ts;
	dt = ts = mill8();

	while(digitalRead(rfm->irq)) {
		dt = mill8() - ts;
		if (dt > timeout)
			return -1;
	}

	return dt;
}

static void rfm12_cmdw(rfm12_t *rfm, uint16_t cmd)
{
	if (!(rfm->mode & RFM_SPI_SELECTED))
		digitalWrite(rfm->cs, LOW);

	if (rfm->mode & RFM_SPI_MODE_HW)
		spi_write_word(cmd);
	else {
		digitalWrite(rfm->sck, LOW);

		for(uint16_t i = 0x8000; i; i >>= 1) {
			digitalWrite(rfm->sdi,  cmd & i);
			digitalWrite(rfm->sck, HIGH);
			digitalWrite(rfm->sck, LOW);
		}
	}
	if (!(rfm->mode & RFM_SPI_SELECTED))
		digitalWrite(rfm->cs, HIGH);
}

uint16_t rfm12_cmdrw(rfm12_t *rfm, uint16_t cmd)
{
	uint16_t rdata;
	if (!(rfm->mode & RFM_SPI_SELECTED))
		digitalWrite(rfm->cs, LOW);

	if (rfm->mode & RFM_SPI_MODE_HW) {
		rdata = spi_read_byte(cmd >> 8);
		rdata <<= 8;
		rdata |= spi_read_byte(cmd >> 8);
	}
	else {
		uint8_t *pr = (uint8_t *)&rdata;
		digitalWrite(rfm->sck, LOW);
		for(uint16_t i = 0x8000; i; i >>= 1) {
			digitalWrite(rfm->sdi,  cmd & i);
			digitalWrite(rfm->sck, HIGH);
			rdata <<= 1;
			if (digitalRead(rfm->sdo))
				*pr |= 0x01;
			digitalWrite(rfm->sck, LOW);
			cmd <<= 1;
		}
	}
	if (!(rfm->mode & RFM_SPI_SELECTED))
		digitalWrite(rfm->cs, HIGH);
	return rdata;
}

void rfm12_set_rate(rfm12_t *rfm, uint8_t rate)
{
	rfm12_cmdw(rfm, RFM12CMD_DRATE | rate);
}

int16_t rfm12_set_freq(rfm12_t *rfm, uint8_t band, double freq)
{
	int16_t k = -1;

	if (band == RFM12_BAND_315)
		k = (int16_t)((freq - 310.0)*400.0);
	else
	if (band == RFM12_BAND_433)
		k = (int16_t)((freq - 430.0)*400.0);
	else
	if (band == RFM12_BAND_868)
		k = (int16_t)((freq - 860.0)*200.0);
	else
	if (band == RFM12_BAND_915)
		k = (int16_t)((freq - 900.0)*400.0/3.0);

	if ((k < 98) || (k > 3903))
		return -1;

	rfm12_cmdw(rfm, RFM12CMD_FREQ | k);
	return k;
}

// Toggle RFM12_EFIFO to restart the synchron pattern recognition.
void rfm12_reset_fifo(rfm12_t *rfm)
{
	rfm12_cmdw(rfm, RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_DRESET);
	rfm12_cmdw(rfm, RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_SYNCFIFO | RFM12_DRESET);
}

int8_t rfm12_init(rfm12_t *rfm, uint8_t syncpat, uint8_t band, double freq, uint8_t rate)
{
	pinMode(rfm->cs, OUTPUT_HIGH);
	pinMode(rfm->sdi, OUTPUT);
	pinMode(rfm->sck, OUTPUT);
	pinMode(rfm->sdo, INPUT);
	pinMode(rfm->irq, INPUT);

	// enable sensitive reset and issue SW reset command
	rfm12_cmdw(rfm, RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_SYNCFIFO);
	rfm12_cmdw(rfm, RFM12CMD_RESET);
	delay(5);

	// we can use fast write here as we do not read nIRQ
	// nIRQ works only when RFM12 is unselected
	digitalWrite(rfm->cs, LOW);
	rfm->mode |= RFM_SPI_SELECTED;

	rfm12_cmdrw(rfm, RFM12CMD_STATUS); // clean up any pending interrupts
	rfm_band = band;
	rfm12_cmdw(rfm, RFM12CMD_CFG | RFM12_EIDR | RFM12_EFIFO | rfm_band | RFM12_120PF);

	// enter idle mode - only oscillator is running
	rfm12_cmdw(rfm, RFM12CMD_PWR | RFM12_ECRYSRAL | RFM12_DCLOCK);

	rfm12_set_freq(rfm, band, freq);
	rfm12_cmdw(rfm, RFM12CMD_DRATE | rate);
	rfm_rxctl = RFM12_BW_67;
	rfm12_cmdw(rfm, RFM12CMD_RX_CTL | RFM12_VDI_FAST | rfm_rxctl);
	rfm12_cmdw(rfm, RFM12CMD_DFILT | RFM12_CR_ALC | RFM12_DQD4);
	rfm12_cmdw(rfm, RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_SYNCFIFO | RFM12_DRESET);
	rfm12_cmdw(rfm, RFM12CMD_SYNCPAT | syncpat);
	rfm_sync = syncpat;
	rfm12_cmdw(rfm, RFM12CMD_AFC | RFM12_AFC_MODE3 | RFM12_RANGELMT3 | RFM12_AFC_FI | RFM12_AFC_OE | RFM12_AFC_EN);
	rfm_txctl = RFM12_FSK_45 | RFM12_OPWR_MAX;
	rfm12_cmdw(rfm, RFM12CMD_TX_CTL | rfm_txctl);
	rfm12_cmdw(rfm, RFM12CMD_PLL | RFM12_MC_5MHZ | RFM12_DDIT | RFM12_PLL_BW86);
	rfm12_cmdw(rfm, RFM12CMD_WAKEUP);
	rfm12_cmdw(rfm, RFM12CMD_LOWDC);
	rfm12_cmdw(rfm, RFM12CMD_LBDCLK);

	rfm12_reset_fifo(rfm);

	digitalWrite(rfm->cs, HIGH);
	rfm->mode &= ~RFM_SPI_SELECTED;

	return 0;
}

void rfm12_set_mode(rfm12_t *rfm, uint8_t mode)
{
	uint16_t cmd = RFM12CMD_PWR | RFM12_DCLOCK; // default: sleep
	cmd |= mode;
	rfm12_cmdw(rfm, cmd);

	rfm->mode &= ~(RFM_MODE_DATA_RX | RFM_MODE_DATA_TX);
	if (mode == RFM_MODE_RX)
		rfm->mode |= RFM_MODE_DATA_RX;
	else if (mode == RFM_MODE_TX)
		rfm->mode |= RFM_MODE_DATA_TX;
}

int8_t rfm12_set_txpwr(rfm12_t *rfm, uint8_t pwr)
{
	if (pwr > RFM12_OPWR_21)
		return -1;
	rfm_txctl &= 0xF0;
	rfm_txctl |= pwr;
	rfm12_cmdw(rfm, RFM12CMD_TX_CTL | rfm_txctl);
	return 0;
}

int8_t rfm12_set_fsk(rfm12_t *rfm, uint8_t fsk)
{
	if (fsk & 0x0F)
		return -1;
	rfm_txctl &= 0x07;
	rfm_txctl |= fsk;
	rfm12_cmdw(rfm, RFM12CMD_TX_CTL | rfm_txctl);
	return 0;
}

int8_t rfm12_set_rxbw(rfm12_t *rfm, uint8_t bw)
{
	bw &= 0xE0;
	if (bw == 0 || bw == 0xE0)
		return -1;
	rfm_rxctl &= 0x1F;
	rfm_rxctl |= (bw & 0xE0);
	rfm12_cmdw(rfm, RFM12CMD_RX_CTL | rfm_rxctl);
	return 0;
}

// very rough Vbat measurements using RFM12 registers
int8_t rfm12_battery(rfm12_t *rfm, uint8_t mode, uint8_t level)
{
	int8_t i;

	rfm12_set_mode(rfm, RFM_MODE_IDLE); // clear LBD bit
	// disable internal register to avoid false interrupts
	// if enabled we will constantly get RGIT interrupt
	rfm12_cmdw(rfm, RFM12CMD_CFG | RFM12_EFIFO | rfm_band | RFM12_120PF);

	// starting from the specified Vbat level go down and check LBD bit
	for(i = level & 0x1F; i >= 0; i--) {
		rfm12_cmdw(rfm, RFM12CMD_STATUS); // clean up any pending interrupts
		rfm12_cmdw(rfm, RFM12CMD_LBDCLK | i); // set LBD level
		// enabling/disabling combinations gives more stable results
		rfm12_set_mode(rfm, RFM12_ELBD | RFM_MODE_IDLE); // set LBD bit
		rfm_wait_irq(rfm, VBAT_TIMEOUT);
		uint16_t status = rfm12_cmdrw(rfm, RFM12CMD_STATUS);
		if (!(status & RFM12_LBD)) {
			i += 1; // LBD is not set, so go back to previous V level
			break;
		}
		rfm12_set_mode(rfm, RFM_MODE_IDLE); // clear LBD bit
	}

	// re-enable internal TX registers
	rfm12_cmdw(rfm, RFM12CMD_CFG | RFM12_EIDR | RFM12_EFIFO | rfm_band | RFM12_120PF);
	// restore mode
	rfm12_set_mode(rfm, mode);

	// to be consistent with the real Vmeter readings +1 is needed
	return (i >= 0) ? i + 1 : -1;
}

int16_t rfm12_poll(rfm12_t *rfm, uint16_t *status)
{
	uint16_t data = rfm12_cmdrw(rfm, RFM12CMD_STATUS);

	if (status)
		*status = data;

	if (data & RFM12_FFIT) {
		data = rfm12_cmdrw(rfm, RFM12CMD_RX_FIFO);
		return (data & 0x00FF);
	}

	return -1;
}

// sends one byte with timeout
static int8_t _rfm_send(rfm12_t *rfm, uint8_t data, uint8_t timeout)
{
	if (rfm_wait_irq(rfm, timeout) != -1) {
		rfm12_cmdw(rfm, RFM12CMD_TX_FIFO | data);
		return 0;
	}

	return -1;
}

static uint8_t rfm_crc8(uint8_t crc, uint8_t data)
{
	return _crc_ibutton_update(crc, data);
}

// transmit data stream in the following format:
// data len - 1 byte
// data     - "len" bytes
// crc      - 1 byte
// for test purposes: if data is NULL string 'AB....' of len bytes will be sent
int8_t rfm12_send(rfm12_t *rfm, void *buf, uint8_t len)
{
	uint8_t *data = (uint8_t *)buf;

	rfm12_set_mode(rfm, RFM_MODE_TX);
	rfm12_cmdrw(rfm, RFM12CMD_STATUS); // clear any interrupts

	// send preamble	
	if (_rfm_send(rfm, 0xAA, rfm_timeout) != 0)
		return -1;

	_rfm_send(rfm, 0xAA, rfm_timeout);
	_rfm_send(rfm, 0xAA, rfm_timeout);
	_rfm_send(rfm, 0x2D, rfm_timeout); // two sync bytes
	_rfm_send(rfm, 0xD4, rfm_timeout);

	uint8_t crc = rfm_crc8(rfm_sync, len);
	_rfm_send(rfm, len, rfm_timeout);

	for(uint8_t i = 0; i < len; i++) {
		uint8_t byte = data ? data[i] : 'A' + i;
		_rfm_send(rfm, byte ^ 0xA5, rfm_timeout); // ^ xA5 to minimize long run of '0' or '1'
		crc = rfm_crc8(crc, byte);
	}
	_rfm_send(rfm, crc, rfm_timeout);

	// dummy tail
	_rfm_send(rfm, 0x55, rfm_timeout);
	_rfm_send(rfm, 0x55, rfm_timeout);

	rfm_wait_irq(rfm, rfm_timeout);
	rfm12_set_mode(rfm, RFM_MODE_IDLE);

	return 0;
}

static inline void puts_hex(uint8_t data, uint8_t delimiter)
{
	uint8_t hex = (data >> 4) + '0';
	if (hex > '9') hex += 7;
	uart_putc(hex);
	hex = (data & 0x0F) + '0';
	if (hex > '9') hex += 7;
	uart_putc(hex);
	uart_putc(delimiter);
}

static int8_t rfm12_validate_data(uint8_t *buf, uint8_t len, uint8_t rcrc, uint8_t dbg)
{
	uint8_t crc = rfm_crc8(rfm_sync, len);
	if (dbg)
		printf_P(PSTR("| len %d crc %02X\n"), len, crc);

	for(uint8_t i = 0; i < len; i++) {
		crc = rfm_crc8(crc, buf[i]);
		if (dbg)
			printf_P(PSTR("data %02X %3u %02X\n"), buf[i], buf[i], crc);
	}

	if (crc == rcrc)
		return 0;

	// wrong crc
	if (dbg)
		printf_P(PSTR("wrong crc %02X must be %02X\n"), crc, rcrc);
	return -1;
}

// if no interrupt detected returns 0
// if data is available high bit is set and data is in the lower byte
// if interrupt detected but no data is available high bit is 0, all
// other bits are set from status register
static uint16_t rfm12_receive(rfm12_t *rfm, uint16_t *pstatus)
{
	if (digitalRead(rfm->irq) == 0) {
		uint16_t data = rfm12_cmdrw(rfm, RFM12CMD_STATUS);
		// return IRQ status is required
		if (pstatus)
			*pstatus = data;
		// read RX FIFO
		if (data & RFM12_FFIT)
			data = rfm12_cmdrw(rfm, RFM12CMD_RX_FIFO);
		// some other interrupt (RX FIFO overrun, for example)
		return data;
	}
	if (rfm->mode & RFM_RX_PENDING)
		return RFM12_WKUP;
	return 0;
}

// receive data, use data len as packet start byte
// if adc is no null then start ADC conversion to read ARSSI
// (RFM12BS supplies analogue RSSI output on one of the capacitors)
uint8_t rfm12_receive_data(rfm12_t *rfm, void *dbuf, uint8_t len, uint8_t flags)
{
	if (!(rfm->mode & RFM_MODE_DATA_RX))
		return 0;

	uint16_t ch;
	uint8_t *buf = (uint8_t *)dbuf;
	uint8_t adc = flags & RFM_RX_ADC_MASK;
	uint8_t dbg = flags & RFM_RX_DEBUG;

	while((ch = rfm12_receive(rfm, NULL)) != 0) {
		// check status for FIFO overflow and for RX timeout
		if (!(ch & 0x8000)) {
			if (ch & RFM12_FFOV) { // FIFO overflow, reset buffer index
				rfm12_reset_fifo(rfm);
				rfm->ridx = 0;
				rfm->mode &= ~RFM_RX_PENDING;
			}
			// check for RX timeout, for 9600 it is ~len*5
			if (rfm->mode & RFM_RX_PENDING) {
				if ((mill8() - rfm->rxts) >= len*5) {
					rfm->ridx = 0;
					rfm->mode &= ~RFM_RX_PENDING;
					ch = 0x7BAD;
				}
			}
			// if RFM_RX_PENDGING is set then rfm12_receive() returns RFM12_WKUP
			// just to keep this while() loop running till all data is received
			if (dbg && (ch != RFM12_WKUP)) {
				uart_putc('s');
				puts_hex(ch >> 8, '-');
				puts_hex(ch, ' ');
			}
			continue;
		}
		if (adc)
			analogStart();
		uint8_t data = (uint8_t)ch;
		if (dbg)
			puts_hex(data, ' ');

		if (rfm->ridx == 0) {
			if (data == len) {
				rfm->ridx++;
				rfm->rxts = mill8();
				rfm->mode |= RFM_RX_PENDING;
			}
			continue;
		}

		if (rfm->ridx == (len + 2)) { // data should contain tail (0x55) now
			rfm->ridx = 0; // reset buffer index
			rfm->mode &= ~RFM_RX_PENDING;
			if (data != 0x55) {
				if (dbg)
					uart_puts_p(PSTR("- wrong tail\n"));
				continue;
			}
			if (rfm12_validate_data(buf, len, rfm->rcrc, dbg) == 0) {
				rfm12_reset_fifo(rfm);
				rfm12_set_mode(rfm, RFM_MODE_IDLE);
				return len;
			}
			// wrong crc
			continue;
		}

		if (rfm->ridx <= len)
			buf[rfm->ridx - 1] = data ^ 0xA5;
		else
			rfm->rcrc = data;
		rfm->ridx++;
	}

	return 0;
}
