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
#ifndef _RFM_12BS_H_
#define _RFM_12BS_H_

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif

//** Configuration settings command
#define RFM12CMD_CFG   0x8000
#define RFM12_EIDR     0x0080 // enable Internal Data Register
#define RFM12_EFIFO    0x0040 // enable FIFO
#define RFM12_BAND_315 0x0000 // 315 MHz
#define RFM12_BAND_433 0x0010 // 433 MHz
#define RFM12_BAND_868 0x0020 // 868 MHz
#define RFM12_BAND_915 0x0030 // 915 MHz
#define RFM12_085PF    0x0000 //  8.5 pF
#define RFM12_090PF    0x0001 //  9.0 pF
#define RFM12_095PF    0x0002 //  9.5 pF
#define RFM12_100PF    0x0003 // 10.0 pF
#define RFM12_105PF    0x0004 // 10.5 pF
#define RFM12_110PF    0x0005 // 11.0 pF
#define RFM12_115PF    0x0006 // 11.5 pF
#define RFM12_120PF    0x0007 // 12.0 pF
#define RFM12_125PF    0x0008 // 12.5 pF
#define RFM12_130PF    0x0009 // 13.0 pF
#define RFM12_135PF    0x000A // 13.5 pF
#define RFM12_140PF    0x000B // 14.0 pF
#define RFM12_145PF    0x000C // 14.5 pF
#define RFM12_150PF    0x000D // 15.0 pF
#define RFM12_155PF    0x000E // 15.5 pF
#define RFM12_160PF    0x000F // 16.0 pF

//** Power management command
#define RFM12CMD_PWR   0x8200
#define RFM12_ERXCHAIN  0x0080 // enable receiver chain - RF front end, baseband, synthesizer, crystal oscillator
#define RFM12_EBASEBAND 0x0040 // enable receiver baseband
#define RFM12_ETXSTART  0x0020 // turn on PLL, power amplifier and start TX if enabled
#define RFM12_ESYNTH    0x0010 // turn on synthesizer
#define RFM12_ECRYSRAL  0x0008 // turn on crystal oscillator
#define RFM12_ELBD      0x0004 // enable low-battery detector
#define RFM12_EWAKEUP   0x0002 // enable wake-up timer
#define RFM12_DCLOCK    0x0001 // disable clock

//** Frequency setting command
#define RFM12CMD_FREQ  0xA000

//** Data rate command
#define RFM12CMD_DRATE 0xC600
#define RFM12_BPS_2400  0x0091
#define RFM12_BPS_4800  0x0047
#define RFM12_BPS_9600  0x0023
#define RFM12_BPS_14400 0x0017
#define RFM12_BPS_38400 0x0008
#define RFM12_BPS_57600 0x0005

//** Receiver Control Command
#define RFM12CMD_RX_CTL 0x9000
#define RFM12_VDI_FAST  0x0400 // valid data indicator signal response time
#define RFM12_VDI_MED   0x0500 // valid data indicator signal response time
#define RFM12_VDI_SLOW  0x0600 // valid data indicator signal response time
#define RFM12_VDI_ON    0x0700 // valid data indicator always on
#define RFM12_BW_400    0x0020 // receiver baseband bandwidth 400 kHz
#define RFM12_BW_340    0x0040 // receiver baseband bandwidth 340 kHz
#define RFM12_BW_270    0x0060 // receiver baseband bandwidth 270 kHz
#define RFM12_BW_200    0x0080 // receiver baseband bandwidth 200 kHz
#define RFM12_BW_134    0x00A0 // receiver baseband bandwidth 134 kHz
#define RFM12_BW_67     0x00C0 // receiver baseband bandwidth 67 kHz
#define RFM12_LNA_0     0x0000 // LNA gain relative to maximum [-dB]
#define RFM12_LNA_6     0x0008 // LNA gain relative to maximum [-dB]
#define RFM12_LNA_14    0x0010 // LNA gain relative to maximum [-dB]
#define RFM12_LNA_20    0x0018 // LNA gain relative to maximum [-dB]
#define RFM12_RSSI_103  0x0000 // RSSI detector threshold [-dBm]
#define RFM12_RSSI_97   0x0001 // RSSI detector threshold [-dBm]
#define RFM12_RSSI_91   0x0002 // RSSI detector threshold [-dBm]
#define RFM12_RSSI_85   0x0003 // RSSI detector threshold [-dBm]
#define RFM12_RSSI_79   0x0004 // RSSI detector threshold [-dBm]
#define RFM12_RSSI_73   0x0005 // RSSI detector threshold [-dBm]

//** TX Configuration Control Command
#define RFM12CMD_TX_CTL 0x9800
#define RFM12_FSK_15    0x0000 // FSK modulation, 15 kHz
#define RFM12_FSK_30    0x0010
#define RFM12_FSK_45    0x0020
#define RFM12_FSK_60    0x0030
#define RFM12_FSK_75    0x0040
#define RFM12_FSK_90    0x0050
#define RFM12_FSK_105   0x0060
#define RFM12_FSK_120   0x0070
#define RFM12_FSK_135   0x0080
#define RFM12_FSK_150   0x0090
#define RFM12_FSK_165   0x00A0
#define RFM12_FSK_180   0x00B0
#define RFM12_FSK_195   0x00C0
#define RFM12_FSK_210   0x00D0
#define RFM12_FSK_225   0x00E0
#define RFM12_FSK_240   0x00F0
#define RFM12_OPWR_MAX  0x0000 // Maximum output power
#define RFM12_OPWR_3    0x0001 // Output power, [-dB]
#define RFM12_OPWR_6    0x0002 // Output power, [-dB]
#define RFM12_OPWR_9    0x0003 // Output power, [-dB]
#define RFM12_OPWR_12   0x0004 // Output power, [-dB]
#define RFM12_OPWR_15   0x0005 // Output power, [-dB]
#define RFM12_OPWR_18   0x0006 // Output power, [-dB]
#define RFM12_OPWR_21   0x0007 // Output power, [-dB]

//** Data Filter Command
#define RFM12CMD_DFILT  0xC228
#define RFM12_CR_ALC    0x0080 // clock recovery (CR) auto lock control
#define RFM12_CR_FAST   0x0040 // clock recovery lock control fast mode
#define RFM12_CR_SLOW   0x0000 // clock recovery lock control slow mode
#define RFM12_DIG_FILT  0x0000 // digital data filter
#define RFM12_RC_FILT   0x0010 // analog RC data filter
#define RFM12_DQD0      0x0000 // DQD threshold parameter 0-7
#define RFM12_DQD1      0x0001
#define RFM12_DQD2      0x0002
#define RFM12_DQD3      0x0003
#define RFM12_DQD4      0x0004
#define RFM12_DQD5      0x0005
#define RFM12_DQD6      0x0006
#define RFM12_DQD7      0x0007

//** Receiver FIFO Read Command
#define RFM12CMD_RX_FIFO 0xB000

//** Transmitter Register Write Command
#define RFM12CMD_TX_FIFO 0xB800

//** Low Duty-Cycle Command
#define RFM12CMD_LOWDC 0xC800
#define RFM12_ELOWDC   0x0001 // Enables the Low Duty-Cycle Mode

//** Low Battery Detector and Microcontroller Clock Divider Command
#define RFM12CMD_LBDCLK 0xC000

//** PLL Setting Command
#define RFM12CMD_PLL    0xCC12
#define RFM12_MC_5MHZ   0x0060 // MC CLK frequency 5 or 10 MHz
#define RFM12_MC_33MHZ  0x0040 // MC CLK frequency 3.3 MHz
#define RFM12_MC_25MHZ  0x0020 // MC CLK frequency 2.5 MHz
#define RFM12_DLY       0x0008 // switches on the delay in the phase detector
#define RFM12_DDIT      0x0004 // disables the dithering in the PLL loop
#define RFM12_PLL_BW86  0x0000 // PLL bandwidth Max bit rate 86.2 [kbps]
#define RFM12_PLL_BW256 0x0001 // PLL bandwidth Max bit rate 256 [kbps]

//** FIFO and Reset Mode Command
#define RFM12CMD_FIFO   0xCA00
#define RFM12_RXBIT8    0x0080 // FIFO generates IT when the number of RX data bits reaches this level
#define RFM12_RXBIT16   0x00F0 // FIFO generates IT when the number of RX data bits reaches this level
#define RFM12_EXSYNC    0x0000 // synchron pattern includes 2Dh as the first byte
#define RFM12_NOSYNC    0x0008 // synchron pattern includes only one byte
#define RFM12_FIFO_AL   0x0004 // FIFO fill start condition always fill, if not set - synchron pattern starts FIFO
#define RFM12_SYNCFIFO  0x0002 // FIFO fill will be enabled after synchron pattern reception. The FIFO fill stops when this bit is cleared.
#define RFM12_DRESET    0x0001 // Disables the highly sensitive RESET mode.

//** Synchron Pattern Command
#define RFM12CMD_SYNCPAT 0xCE00

//** AFC Command
#define RFM12CMD_AFC     0xC400
#define RFM12_AFC_MODE0  0x0000 // Auto mode off (Strobe is controlled by micro controller)
#define RFM12_AFC_MODE1  0x0040 // Runs only once after each power-up
#define RFM12_AFC_MODE2  0x0080 // Keep the Foffset only during receiving
#define RFM12_AFC_MODE3  0x00C0 // Keep the Foffset value
#define RFM12_RANGELMT0  0x0000 // No restriction
#define RFM12_RANGELMT1  0x0010 // +15 fres to -16 fres
#define RFM12_RANGELMT2  0x0020 // +7 fres to -8 fres
#define RFM12_RANGELMT3  0x0030 // +3 fres to -4 fres
#define RFM12_AFC_ST     0x0008 // strobe edge
#define RFM12_AFC_FI     0x0004 // switch the circuit to high accuracy (fine) mode
#define RFM12_AFC_OE     0x0002 // enables the frequency offset register
#define RFM12_AFC_EN     0x0001 // enables the calculation of the offset frequency by the AFC circuit

//** Wake-Up Timer Command
#define RFM12CMD_WAKEUP 0xE000
//* Special combination for SW reset
#define RFM12CMD_RESET  0xEF00

//** Status read command
#define RFM12CMD_STATUS 0x0000
#define RFM12_RGIT     0x8000 // TX register is ready to receive the next byte
#define RFM12_FFIT     0x8000 // The number of data bits in the RX FIFO has reached the pre-programmed limit
#define RFM12_POR      0x4000 // Power-on reset (Cleared on Read)
#define RFM12_RGUR     0x2000 // TX register under run, register over write (Cleared on Read)
#define RFM12_FFOV     0x2000 // RX FIFO overflow (Cleared on Read)
#define RFM12_WKUP     0x1000 // Wake-up timer overflow (Cleared on Read)
#define RFM12_EXT      0x0800 // Logic level on interrupt pin changed to low (Cleared on Read)
#define RFM12_LBD      0x0400 // Low battery detect, the power supply voltage is below the pre-programmed limit
#define RFM12_FFEM     0x0200 // FIFO is empty
#define RFM12_ATS      0x0100 // Antenna tuning circuit detected strong enough RF signal
#define RFM12_RSSI     0x0100 // The strength of the incoming signal is above the pre-programmed limit
#define RFM12_DQD      0x0080 // Data quality detector output
#define RFM12_CRL      0x0040 // Clock recovery locked
#define RFM12_ATGL     0x0020 // Toggling in each AFC cycle
#define RFM12_OFFS     0x0010 // MSB of the measured frequency offset (sign of the offset value)
#define RFM12_OFFSET   0x000F // Offset value to be added to the value of the frequency control parameter (Four LSB bits)

//** Power mode - sleep, idle, receiver, transmitter
#define RFM_MODE_SLEEP 0
#define RFM_MODE_IDLE  RFM12_ECRYSRAL
#define RFM_MODE_RX    RFM12_ERXCHAIN
#define RFM_MODE_TX    RFM12_ETXSTART

// initializes RFM12 and puts it to idle mode 
int8_t   rfm12_init(uint8_t band, double freq, uint8_t rate);
uint16_t rfm12_cmd(uint16_t cmd); // RFM12 command

// some of 'set' functions
void    rfm12_set_mode(uint8_t mode); // set working mode RFM_MODE_* above
int16_t rfm12_set_freq(uint8_t band, double freq); // set band and frequency
int8_t  rfm12_set_txpwr(uint8_t pwr); // set output power, RFM12_OPWR_* (0-7)
int8_t  rfm12_set_fsk(uint8_t fsk); // set FSK modulation, RFM12_FSK_* above
// set receiver baseband bandwidth, one of RFM12_BW_* above
int8_t  rfm12_set_rxbw(uint8_t bw);
// rate - one of RFM12_BPS_* above
static inline void rfm12_set_rate(uint8_t rate)
{
	rfm12_cmd(RFM12CMD_DRATE | rate);
}

// very rough battery voltage measurement using Low Battery Detector
int8_t rfm12_battery(uint8_t mode, uint8_t level);

void   rfm12_reset_fifo(void); // reset RX FIF buffer
// poll RX FIFO (does no use nIRQ)
// get status register and returns data byte if available
int16_t rfm12_poll(uint16_t *status);

// check if data available (nIRQ is LOW) and read data byte
uint16_t rfm12_receive(uint16_t *status);
// receive data stream
uint8_t rfm12_receive_data(void *buf, uint8_t len, uint16_t *arssi);
int8_t  rfm12_send(uint8_t *data, uint8_t len); // transmit data stream

#ifdef __cplusplus
}
#endif

#endif

