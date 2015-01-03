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

#include "timer.h"
#include "rfm12bs.h"
#include "mmr70pin.h"

// Interface to RFM12B
#define RF_SCK TP10 // SPI clock
#define RF_SDO TP7  // MOSI
#define RF_SDI TP3  // MISO
#define RF_SS  EXSS // SPI SS (slave select)
#define nIRQ   EXINT1

#define SPI_MODE_SW 0
#define SPI_MODE_HW 1

#define SPI_MODE SPI_MODE_SW

static uint8_t rfm_timeout = 50; // default TX timeout
static uint8_t rfm_txctl;
static uint8_t rfm_rxctl;

// waits timeout msecs for nIRQ pin to go to LOW state
static int8_t rfm_wait_irq(uint8_t timeout)
{
	uint16_t ts = mill16();

	while(_pin_get(&PIND, _BV(nIRQ))) {
		if ((mill16() - ts) > timeout)
			return -1;
	}

	return 0;
}

#if (SPI_MODE == SPI_MODE_SW)

// simple SW bit-bang SPI
uint16_t rfm12_cmd(uint16_t cmd)
{
	uint16_t recv = 0;

	_pin_set(&PORTB, _BV(RF_SCK), LOW);
	_pin_set(&PORTB, _BV(RF_SS), LOW);

	for(uint8_t i = 0; i < 16; i++) {
		if (cmd & 0x8000)
			_pin_set(&PORTB, _BV(RF_SDI), HIGH);
		else
			_pin_set(&PORTB, _BV(RF_SDI), LOW);
		_pin_set(&PORTB, _BV(RF_SCK), HIGH);
		recv <<= 1;
		if (_pin_get(&PINB, _BV(RF_SDO)))
			recv |= 0x0001;
		_pin_set(&PORTB, _BV(RF_SCK), LOW);
		cmd <<= 1;
	}

	_pin_set(&PORTB, _BV(RF_SS), HIGH);
	return recv;
}

#else

// HW SPI, to be added
uint16_t rfm12_cmd(uint16_t cmd)
{
	uint16_t recv = 0;

	_pin_set(&PORTB, _BV(RF_SS), LOW);
	cmd = cmd;
	_pin_set(&PORTB, _BV(RF_SS), HIGH);
	return recv;
}

#endif

int16_t rfm12_set_freq(uint8_t band, double freq)
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

	rfm12_cmd(RFM12CMD_FREQ | k);
	return k;
}

// Toggle RFM12_EFIFO to restart the synchron pattern recognition.
void rfm12_reset_fifo(void)
{
	rfm12_cmd(RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_DRESET);
	rfm12_cmd(RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_SYNCFIFO | RFM12_DRESET);
}

int8_t rfm12_init(uint8_t band, double freq, uint8_t rate)
{
	_pin_mode(&DDRB, _BV(RF_SS), OUTPUT_HIGH);
	_pin_mode(&DDRB, _BV(RF_SDI), OUTPUT_HIGH);
	_pin_mode(&DDRB, _BV(RF_SCK), OUTPUT_LOW);
	_pin_dir(&DDRD, _BV(nIRQ), INPUT);

	rfm12_cmd(RFM12CMD_STATUS); // clean up any pending interrupts
	rfm12_cmd(RFM12CMD_CFG | RFM12_EIDR | RFM12_EFIFO | band | RFM12_120PF);
	// enter idle mode - only oscillator is running
	rfm12_cmd(RFM12CMD_PWR | RFM12_ECRYSRAL | RFM12_DCLOCK);

	rfm12_set_freq(band, freq);
	rfm12_cmd(RFM12CMD_DRATE | rate);
	rfm_rxctl = RFM12_BW_67;
	rfm12_cmd(RFM12CMD_RX_CTL | RFM12_VDI_FAST | rfm_rxctl);
	rfm12_cmd(RFM12CMD_DFILT | RFM12_CR_ALC | RFM12_DQD4);
	rfm12_cmd(RFM12CMD_FIFO | RFM12_RXBIT8 | RFM12_SYNCFIFO | RFM12_DRESET);
	rfm12_cmd(RFM12CMD_SYNCPAT | 0xD4);
	rfm12_cmd(RFM12CMD_AFC | RFM12_AFC_MODE3 | RFM12_RANGELMT3 | RFM12_AFC_FI | RFM12_AFC_OE | RFM12_AFC_EN);
	rfm_txctl = RFM12_FSK_45 | RFM12_OPWR_MAX;
	rfm12_cmd(RFM12CMD_TX_CTL | rfm_txctl);
	rfm12_cmd(RFM12CMD_PLL | RFM12_MC_5MHZ | RFM12_DDIT | RFM12_PLL_BW86);
	rfm12_cmd(RFM12CMD_WAKEUP);
	rfm12_cmd(RFM12CMD_LOWDC);
	rfm12_cmd(RFM12CMD_LBDCLK);

	rfm12_reset_fifo();
	return 0;
}

void rfm12_set_mode(uint8_t mode)
{
	uint16_t cmd = RFM12CMD_PWR | RFM12_DCLOCK; // default: sleep
	cmd |= mode;
	rfm12_cmd(cmd);
}

int8_t rfm12_set_txpwr(uint8_t pwr)
{
	if (pwr > RFM12_OPWR_21)
		return -1;
	rfm_txctl &= 0xF0;
	rfm_txctl |= pwr;
	rfm12_cmd(RFM12CMD_TX_CTL | rfm_txctl);
	return 0;
}

int8_t rfm12_set_fsk(uint8_t fsk)
{
	if (fsk & 0x0F)
		return -1;
	rfm_txctl &= 0x07;
	rfm_txctl |= fsk;
	rfm12_cmd(RFM12CMD_TX_CTL | rfm_txctl);
	return 0;
}

int8_t rfm12_set_rxbw(uint8_t bw)
{
	bw &= 0xE0;
	if (bw == 0 || bw == 0xE0)
		return -1;
	rfm_rxctl &= 0x1F;
	rfm_rxctl |= (bw & 0xE0);
	rfm12_cmd(RFM12CMD_RX_CTL | rfm_rxctl);
	return 0;
}

int8_t rfm12_battery(uint8_t mode, uint8_t level)
{
	int8_t i;
	mode = mode;
	
	// disable RX/TX interrupts and enable Low Battery Detector
	rfm12_set_mode(RFM12_ELBD | RFM_MODE_IDLE);
	rfm12_cmd(RFM12CMD_STATUS); // clean up any pending interrupts

	// staring from specified Vbat level go down and check LBD bit
	for(i = (level & 0x1F); i >= 0; i--) {
		rfm12_cmd(RFM12CMD_LBDCLK | i);
		rfm_wait_irq(10);
		uint16_t status = rfm12_cmd(RFM12CMD_STATUS);
		if (!(status & RFM12_LBD)) {
			i += 1;
			break;
		}
	}

	// restore mode
	rfm12_set_mode(mode);
	return (i > 0) ? i : -1;
}

int16_t rfm12_poll(uint16_t *status)
{
	uint16_t data = rfm12_cmd(RFM12CMD_STATUS);

	if (status)
		*status = data;

	if (data & RFM12_FFIT) {
		data = rfm12_cmd(RFM12CMD_RX_FIFO);
		return (data & 0x00FF);
	}

	return -1;
}

// if no interrupt detected returns 0
// if data is available high bit is set and data is in the lower byte
// if interrupt detected but no data is available high bit is 0, all
// other bits are set from status register
uint16_t rfm12_receive(uint16_t *status)
{
	uint16_t data = 0;

	if (_pin_get(&PIND, _BV(nIRQ)) == 0) {
		// return IRQ status is required
		if (status)
			*status = rfm12_cmd(RFM12CMD_STATUS);
		// read RX FIFO
		data = rfm12_cmd(RFM12CMD_RX_FIFO);
		// high bits are set if data is available
		if (data & 0x8000)
			return (data & 0x80FF);
		// some other interrupt (RX FIFO overrun, for example)
		if (status)
			data = *status;
		else
			data = rfm12_cmd(RFM12CMD_STATUS); // clear interrupts flag
		data &= 0x7FFF;
	}

	return data;
}

// sends one byte with timeout
static int8_t _rfm_send(uint8_t data, uint8_t timeout)
{
	if (rfm_wait_irq(timeout) == 0) {
		rfm12_cmd(RFM12CMD_TX_FIFO | data);
		return 0;
	}

	return -1;
}

// reuse CRC calculation from SHT1x code
extern uint8_t sht1x_crc(uint8_t data, uint8_t seed);

// transmit data stream in the following format:
// data length - 1 byte
// data        - len bytes
// crc         - 1 byte
// for test purposes: if data is NULL string 'AB....' of len bytes will be sent
int8_t rfm12_send(uint8_t *data, uint8_t len)
{
	rfm12_cmd(RFM12CMD_STATUS); // clear any interrupts
	rfm12_set_mode(RFM_MODE_TX);

	// send preamble	
	if (_rfm_send(0xAA, rfm_timeout) != 0)
		return -1;

	_rfm_send(0xAA, rfm_timeout);
	_rfm_send(0xAA, rfm_timeout);
	_rfm_send(0x2D, rfm_timeout); // two sync bytes
	_rfm_send(0xD4, rfm_timeout);

	uint8_t crc = sht1x_crc(len, len);
	_rfm_send(len, rfm_timeout);

	for(uint8_t i = 0; i < len; i++) {
		uint8_t byte = data ? data[i] : 'A' + i;
		_rfm_send(byte, rfm_timeout);
		crc = sht1x_crc(byte, crc);
	}
	_rfm_send(crc, rfm_timeout);

	// dummy tail
	_rfm_send(0x55, rfm_timeout);
	_rfm_send(0x55, rfm_timeout);

	rfm_wait_irq(rfm_timeout);
	rfm12_set_mode(RFM_MODE_IDLE);

	return 0;
}

// receive data, use data len as packet start byte
// if *arssi is not NULL, then it contains ADC channel to read ARSSI value from
// (RFM12BS supplies analogue RSSI output on one of the capacitors)
uint8_t rfm12_receive_data(void *dbuf, uint8_t len, uint16_t *arssi)
{
	static uint8_t ridx = 0;
	uint8_t *buf = (uint8_t *)dbuf;

	uint16_t ch;
	while(((ch = rfm12_receive(NULL)) & 0x8000)) {
		uint8_t data = (uint8_t)ch;
		if (ridx == 0) {
			if (arssi && *arssi < 8) {
				*arssi = analogRead(*arssi);
				*arssi |= 0x8000;
			}
			if (data == len)
				ridx++;
			continue;
		}
		if (ridx == (len + 1)) { // data should contain CRC now
			ridx = 0; // reset buffer index
			rfm12_reset_fifo();

			uint8_t crc = sht1x_crc(len, len);
			for(uint8_t i = 0; i < len; i++)
				crc = sht1x_crc(buf[i], crc);
			if (crc == data)
				return len;
			// wrong crc
			continue;
		}
		buf[ridx - 1] = data;
		ridx++;
	}

	if (ch & RFM12_FFOV) { // FIFO overflow, reset buffer index
		rfm12_reset_fifo();
		ridx = 0;
	}

	return 0;
}
