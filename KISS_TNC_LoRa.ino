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
#define _FEND	0xC0
#define _FESC	0xDB
#define _TFEND	0xDC
#define _TFESC	0xDD

/* KISS frame type */
enum kiss_frame_type
{
	_FRAME,
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

void setup(void)
{
	Serial.begin(9600);
	XCVR.begin(9600);

	pinMode(2, INPUT);
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
			while(Serial.available() <= 0)
				spin();
			break;
		default:
			break;
	}
}

void loop()
{
	uint8_t c;
	uint8_t state = _END;
	char debug_buf[256];

	while(1)
	{
		while((Serial.available() <= 0 && XCVR.available() <= 0))
			spin();
		
		/* Computer -> Modem -> RF */
		if(Serial.available() > 0)
		{
			c = Serial.read();

			if(state != _START)
			{
				switch(c)
				{
					case _FEND:
						if(state == _END)
						{
							state = _START;
						}
						else
						{
							state = _END;
						}
						break;
					case _FESC:
						if(state == _DATA)
						{
							state = _ESCAPE;
						}
						break;
					case _TFEND:
					case _TFESC:
						if(state == _ESCAPE)
							state = _DATA;
						break;
					default:
						state = _END;
						break;
				}
			}
			else
			{
				if(c == OP(0x0, _FRAME))	/* First port, data frame */
				{
					XCVR.write("\xC0\x00", 2); /* specifiy that it's 2 bytes, otherwise it will be ignored (NUL) */
					do
					{
						wait(WAIT_COM);
						XCVR.write(c = Serial.read());
					}
					while(c != _FEND);
					wait(WAIT_AUX);

					state = _END;
				}
				else
				{
					while(c != _FEND)	/* Skip this frame */
					{
						wait(WAIT_COM);
						c = Serial.read();
					}
					state = _END;
				}
			}
		}
		
		/* RF -> Modem -> Computer */
		if(XCVR.available() > 0)
			Serial.write(XCVR.read());
	}
}
