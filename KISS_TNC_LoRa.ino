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
#include <stdio.h>
#include <SoftwareSerial.h>

/* Pin Configuration */
#define AUX	2
#define XCVR_RX	11
#define XCVR_TX	12

/* State machine */
enum kiss_state
{
	_START,
	_DATA,
	_ESCAPE,
	_END
};

/* Special characters */
#define _FEND	((byte)0xC0)
#define _FESC	((byte)0xDB)
#define _TFEND	((byte)0xDC)
#define _TFESC	((byte)0xDD)
#define _AX25_FLAG	((byte)0x7E)

/* KISS frame type */
enum kiss_frame_type
{
	_FRAME = 0,
	_TXDELAY,
	_P,
	_SLOTTIME,
	_FDUPLEX,
	_SETHARDWARE,
	_RETURN = 0xFF
};

#define OP(dev, type) \
	((dev << 4 & 0xF0) | type)

/* SoftwareSerial for connecting UART LoRa transceiver */
SoftwareSerial XCVR(XCVR_RX, XCVR_TX);
#define COM Serial

void setup(void)
{
	COM.begin(9600);
	XCVR.begin(9600);

	pinMode(AUX, INPUT_PULLUP);
	pinMode(LED_BUILTIN, OUTPUT);
}

/* Idle spinning */

void spin(void)
{
	digitalWrite(LED_BUILTIN, digitalRead(AUX));
}

enum wait_type
{
	WAIT_COM,
	WAIT_XCVR,
	WAIT_AUX
};

/* Waiting for data or operation completion */
void wait(int type)
{
	switch(type)
	{
		case WAIT_AUX:
			while(digitalRead(AUX) == LOW)
				spin();
			break;
		case WAIT_XCVR:
			while(XCVR.available() <= 0)
				spin();
			break;
		case WAIT_COM:
			while(COM.available() <= 0)
				spin();
			break;
		default:
			break;
	}
}

void loop()
{
	uint8_t c;
	uint8_t tx_state = _END;
	uint8_t rx_state = _END;
	char debug_buf[256];

while(1)
{
	while((Serial.available() <= 0 && XCVR.available() <= 0))
		spin();

	/* Computer -> Modem -> RF */
	if(COM.available() > 0)
	{
		c = COM.read();

		if(tx_state != _START)
		{
			switch(c)
			{
				case _FEND:
					if(tx_state == _END)
					{
						tx_state = _START;
					}
					else
					{
						tx_state = _END;
					}
					break;
				case _FESC:
					if(tx_state == _DATA)
					{
						tx_state = _ESCAPE;
					}
					break;
				case _TFEND:
				case _TFESC:
					if(tx_state == _ESCAPE)
						tx_state = _DATA;
					break;
				default:
					tx_state = _END;
					break;
			}
		}
		else
		{
			if(c == OP(0x0, _FRAME))	/* First port, data frame */
			{
				XCVR.write(_AX25_FLAG);
				while(c != _FEND)
				{
					wait(WAIT_COM);
					c = COM.read();
					if(c == _FEND)
						break;
					else
						XCVR.write(c);
				}
				XCVR.write(_AX25_FLAG);
				wait(WAIT_AUX);

				tx_state = _END;
			}
			else
			{
				{
					while(COM.available() <= 0) spin();
					c = COM.read();
				}
				while(c != _FEND)	/* Skip this frame, also skips FENDs (one at a time) */
				tx_state = _END;
			}
		}
	}

	/* RF -> Modem -> Computer */
	if(XCVR.available() > 0)
	{
		c = XCVR.read();
		switch(c)
		{
			case _FEND:
			case _FESC:
				COM.write(_FESC);
				if(c == _FEND)
					COM.write(_TFEND);
				else if(c == _FESC)
					COM.write(_TFESC);
				break;
			case _AX25_FLAG:
				if(rx_state == _END)
				{
					rx_state = _DATA;
					COM.write("\xC0\x00", 2);
				}
				else if(rx_state == _DATA)
				{
					rx_state = _END;
					COM.write('\xC0');
				}
				break;
			default:
				if(rx_state != _END) /* Do not pass stray data outside of frame */
					COM.write(c);
				break;
		}
	}
}
}
