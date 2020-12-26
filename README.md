# KISS TNC for UART LoRa module as Packet Radio Modem
This project is using Arduino as an UART interface between computer and UART LoRa module, which transmits UART character stream directly using LoRa modulation.  
The main usage is to transmit APRS packet over LoRa.

## Operating Principle
By violating the specification of the KISS protocol (ignoring all non-data frames), and directly relaying the KISS data frame coming from LoRa module, we avoided implementing all KISS functionalities. Thus, less code, less work.  

I am aware that LoRa is not a reliable media, and lost LoRa might cause APRS packet being truncated, but it seems to work fine. (as long as one APRS packet fits in one LoRa packet)

# On Air Format
Same as AX.25, but 16 bit checksum is omitted.

## Credits
BX4ACP for giving me advises
