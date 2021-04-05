/* ========================================================================== *\
||		KISS TNC for UART LoRa module as Packet Radio Modem	      ||
||                                 Neo_Chen                                   ||
\* ========================================================================== */

/* ========================================================================== *\
|| KISS TNC for UART LoRa module as Packet Radio Modem			      ||
|| Copyright (C) 2020 Kelei Chen					      ||
||									      ||
|| This program is free software: you can redistribute it and/or modify	      ||
|| it under the terms of the GNU General Public License as published by	      ||
|| the Free Software Foundation, either version 3 of the License, or	      ||
|| (at your option) any later version.					      ||
||									      ||
|| This program is distributed in the hope that it will be useful,	      ||
|| but WITHOUT ANY WARRANTY; without even the implied warranty of	      ||
|| MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	      ||
|| GNU General Public License for more details.				      ||
||									      ||
|| You should have received a copy of the GNU General Public License	      ||
|| along with this program.  If not, see <https://www.gnu.org/licenses/>.     ||
\* ========================================================================== */

#include <stdint.h>
#include <SPI.h>
#include <RadioLib.h>
#include "minilzo.h"

/* Special characters */
#define FEND	((byte)0xC0)
#define FESC	((byte)0xDB)
#define TFEND	((byte)0xDC)
#define TFESC	((byte)0xDD)
#define AX25_FLAG	((byte)0x7E)
#define _MAX_PACKET_SIZE	332

/* KISS frame type */
enum kiss_frame_type
{
	_FRAME = 0,
	_TXDELAY,
	_PERSISTANCE,
	_SLOTTIME,
	_FDUPLEX,
	_SETHARDWARE,
	_RETURN = 0xFF
};

SPIClass * hspi = new SPIClass(HSPI);
// NSS, DIO1, NRST, BUSY, SPI
SX1268 radio = new Module(15, 33, 23, 39, *hspi);

volatile int rx_flag = false;

void set_rx_flag(void)
{
	rx_flag = true;
	return;
}

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

void setup(void)
{
	Serial.begin(9600);

	pinMode(LED_BUILTIN, OUTPUT);

	hspi->begin();
	radio.begin(438.2);
	radio.setRfSwitchPins(17, 4);
	radio.setOutputPower(22);
	radio.setBandwidth(62.5);
	radio.setCodingRate(8);
	radio.setSpreadingFactor(8);
	radio.setPreambleLength(32);
	radio.autoLDRO();
	radio.setCRC(2); // 2 byte CRC16

	lzo_init();

	radio.setDio1Action(set_rx_flag);
	radio.startReceive();
}

/* Idle spinning */

void spin(void)
{
}

enum wait_type
{
	WAIT_COM,
	WAIT_XCVR,
};

/* Waiting for data or operation completion */
void wait(int type)
{
	switch(type)
	{
		case WAIT_XCVR:
			break;
		case WAIT_COM:
			while(Serial.available() <= 0)
				spin();
			break;
		default:
			break;
	}
}

/* State machine */
enum kiss_state
{
	// Return Value
	IDLE,
	READY,

	// Internal
	_END,
	_DATA,
	_DISCARD
};

/* Receive KISS frame from computer, note that we don't unescape bytes */
int rx_kiss(int *rx_bytes, byte *d, byte c)
{
	static int state = _END;
	static byte b[2]; /* A two byte buffer so we can look back */

	b[0] = b[1];
	b[1] = c;

	if(b[0] == FEND && b[1] == FEND) // Two FEND is between frames
	{
		*rx_bytes = 0;
		state = _END;
	}
	if(b[0] == FEND && b[1] != FEND)
	{
		if(b[1] == 0)
		{
			state = _DATA;
			*rx_bytes = 0;
		}
		else
			state = _DISCARD;
	}
	if(state == _DATA)
	{
		if(b[1] != FEND)
			d[(*rx_bytes)++] = b[1];
		else
		{
			state = _END;
			return READY;
		}
	}
	if(state == _DISCARD)
	{
		if(b[1] == FEND)
			state = _END;
	}
	return IDLE;
}

#define _MAX_LORA_SIZE 253

void loop()
{
	byte tx_buf[1024];
	byte tx_comp_buf[_MAX_LORA_SIZE];
	int tx_len = 0;

	byte rx_buf[_MAX_LORA_SIZE];
	unsigned char rx_decomp_buf[_MAX_PACKET_SIZE];
	int lzo_errno;
	int state;
	lzo_uint rx_decomp_len=0;
	lzo_uint rx_len = 0;

	/* Receive KISS frame from computer & transmit */
	/*
	if(Serial.available() > 0)
		state = rx_kiss(&tx_len, tx_buf, Serial.read());

	if(state == READY)
	{
		radio.transmit(tx_buf, 255);
	}
	Serial.print(F("Datarate:\t"));
	Serial.print(radio.getDataRate());
	Serial.println(F(" bps"));
	*/
	/* Receive LoRa packet */

	if(rx_flag)
	{
		rx_flag = false;
		rx_len = radio.getPacketLength();
		radio.readData(rx_buf, rx_len);

		lzo_errno = lzo1x_decompress(rx_buf, rx_len, rx_decomp_buf, &rx_decomp_len, NULL);

		if(lzo_errno == LZO_E_OK)
		{
			Serial.write(FEND);
			Serial.write('\0');
			Serial.write(rx_decomp_buf, rx_decomp_len);
			Serial.write(FEND);
		}
		radio.startReceive();
	}
}
