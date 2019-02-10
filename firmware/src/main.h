#ifndef MAIN_h
#define MAIN_h

#include <Arduino.h>
#include <Wire.h>
#include "../lib/Adafruit-MCP23017-Arduino-Library/Adafruit_MCP23017.h"
#include "../lib/dfminimp3/DFMiniMp3.h"
#include "cardlist.h"
#include "WiFi.h"



// defines here
#define MCP_IRQ_PIN 14 // TODO .. which g14=d5 ?
#define SDA_PIN     5 // gpio 5 == d1
#define SCL_PIN     4 // gpio 4 == d2

// port A - inputs
#define MCP_PIN_UP            0	// low active
#define MCP_PIN_DOWN          1	// low active
#define MCP_PIN_LEFT          2	// low active
#define MCP_PIN_RIGHT         3	// low active
#define MCP_PIN_PLAY          4	// low active
#define MCP_PIN_BUSY          5 // low = playing
#define MCP_PIN_PN532_IRQ   6
// port B - outputs
#define MCP_PIN_PN532_RESET 7
#define MCP_PIN_POWER_SWITCH  8

#define STATE_IDLE												0
#define STATE_NEW_CARD 										1
#define STATE_REGULAR_PLAYBACK 						2
#define STATE_UNKNOWN_CARD_INTRO 					3
#define STATE_UNKNOWN_CARD_MODE						100
#define STATE_UNKNOWN_CARD_FOLDER					110
#define STATE_UNKNOWN_CARD_FOLDER_POST_1	111
#define STATE_UNKNOWN_CARD_TRACK					120
#define STATE_UNKNOWN_CARD_TRACK_POST_1		121
#define STATE_UNKNOWN_CARD_STORE					130
#define STATE_UNKNOWN_CARD_STORE_POST_1		131

#define MODE_SINGLE_RANDOM_TRACK 					1
#define MODE_COMPLETE_FOLDER 							2
#define MODE_RANDOM_FOLDER 								3
#define MODE_SINGLE_TRACK 								4
#define MODE_COMPLETE_FOLDER_CONTINUUES 	5
#define MODE_ADMIN_CARD 									6

#define PLAY_TYP_FIRST										0
#define PLAY_TYP_NEXT											1
#define PLAY_TYP_PREV											2

// declare vars here
uint16_t temp_16;
uint8_t state;
uint8_t gpio_state  = 0xff;
uint8_t gpio_down   = 0;
uint8_t gpio_pushed = 0;
uint32_t gpio_first_pushed[8];
bool mcp_interrupt_pending = true;

bool power_down_after_track = false;
uint8_t max_volume = 0;
uint32_t power_down_at_ts = 0;

uint32_t last_seen_card = 0;
uint32_t last_asked_for_card = 0;
uint8_t ask_for_card = 0;

uint8_t mp3_voice_menu_status = 0;

uint8_t uid[10];  // Buffer to store the returned UID
uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
Adafruit_MCP23017 mcp;
list cardList;
listElement* card_scanned;
listElement* card_found;
DFMiniMp3 mp3;

void play(uint8_t typ);
void powerDown(const __FlashStringHelper* log);
bool voiceMenu(uint16_t* data, uint8_t max, uint16_t offset, uint8_t* status);
uint8_t key_menu();





/*
 * 01	VDDA			Analog Power 3.0 V ~ 3.6V
 * 02	LNA				RF Antenna Interface Chip Output Impedance = 50 Ω No matching required. It is suggested to retain the π-type matching network to match the antenna.
 * 03	VDD3P3		Amplifier power: 3.0 V ~ 3.6 V
 * 04	VDD3P3		Amplifier power:3.0 V ~ 3.6 V
 * 05	VDD_RTC		NC (1.1 V)
 * 06	TOUT	I		ADC pin. It can be used to test the power-supply voltage of VDD3P3 (Pin3 and Pin4) and the input power voltage of TOUT (Pin 6). However, these two functions cannot be used simultaneously
 * 07	CHIP_EN		Chip Enable, High: On, chip works properly, Low: Off, small current consumed
 * 08	GPIO16		D0,XPD_DCDC, Deep-sleep wakeup (need to be connected to EXT_RSTB;
 *
 * 09	GPIO14		D5; MTMSl; HSPI_CLK
 * 10	GPIO12		D6; MTDI; HSPI_MISO 						// DO NOT USE, CONNECTED TO FLASH
 * 11	VDDPST		P	Digital / IO power supply (1.8 V ~ 3.3 V)
 * 12	GPIO13		D7; MTCK; HSPI_MOSI; UART0_CTS
 * 13	GPIO15		D8; MTDO; HSPI_CS; UART0_RTS  	// DO NOT USE, on board pull down
 * 14	GPIO02		D4; internal LED
 * 15	GPIO00		D3; SPI_CS2; Button pin, needs to be high during startup, will be checked 10 sec afterboot, if low-> wifimanager start
 * 16	GPIO04		D2
 *
 * 17	VDDPST		Digital / IO power supply (1.8 V ~ 3.3 V)
 * 18	GPIO09		SDIO_DATA_2; Connect to SD_D2 (Series R: 200 Ω); SPIHD; HSPIHD // DO NOT USE, CONNECTED TO FLASH
 * 19	GPIO10		SDIO_DATA_3; Connect to SD_D3 (Series R: 200 Ω); SPIWP; HSPIWP // DO NOT USE, CONNECTED TO FLASH
 * 20	GPIO11		SDIO_CMD;	Connect to SD_CMD (Series R: 200 Ω); SPI_CS0;	// DO NOT USE, CONNECTED TO FLASH
 * 21	GPIO06		SDIO_CLK; Connect to SD_CLK (Series R: 200 Ω); SPI_CLK;	// DO NOT USE, CONNECTED TO FLASH
 * 22	GPIO07		SDIO_DATA_0; Connect to SD_D0 (Series R: 200 Ω); SPI_MSIO; // DO NOT USE, CONNECTED TO FLASH
 * 23	GPIO08		SDIO_DATA_1; Connect to SD_D1 (Series R: 200 Ω); SPI_MOSI; // DO NOT USE, CONNECTED TO FLASH
 * 24	GPIO05		D1
 *
 * 25 GPIO03		D9; U0RXD; UART Rx during flash programming;
 * 26	GPIO01		D10; U0TXD; UART Tx during flash programming; SPI_CS1
 * 27	XTAL_OUT	Connect to crystal oscillator output,	can be used to provide BT clock input
 * 28	XTAL_IN		Connect to crystal oscillator input
 * 29	VDDD			Analog power 3.0 V ~ 6 V
 * 30	VDDA			Analog power 3.0 V ~ 3.6 V
 * 31	RES12K		Serial connection with a 12 kΩ resistor to the ground
 * 32	EXT_RSTB	External reset signal (Low voltage level: Active)
 */
#endif
