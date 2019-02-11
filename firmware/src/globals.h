#ifndef GLOBAL_h
#define GLOBAL_h

#include "../lib/Adafruit-MCP23017-Arduino-Library/Adafruit_MCP23017.h"
#include "../lib/dfminimp3/DFMiniMp3.h"
#include "cardlist.h"
#include "settings.h"
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>
#include "../lib/PubSubClient/src/PubSubClient.h"

// defines here
#define DEBUG 1
#ifdef DEBUG
	#define DMSG(args...)					Serial.print(args)
	#define DMSG_LN(str)					Serial.println(str)
	#define DMSG_F(args...)				Serial.printf(args)
#else
	#define DMSG(args...)
	#define DMSG_LN(str)
	#define DMSG_F(args...)
#endif

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
#define STATE_UNKNOWN_CARD_ADMIN_MODE			140
#define STATE_UNKNOWN_CARD_ADMIN_VALUE		141

#define MODE_SINGLE_RANDOM_TRACK 					1
#define MODE_COMPLETE_FOLDER 							2
#define MODE_RANDOM_FOLDER 								3
#define MODE_SINGLE_TRACK 								4
#define MODE_COMPLETE_FOLDER_CONTINUUES 	5
#define MODE_ADMIN_CARD 									6

#define PLAY_TYP_FIRST										0
#define PLAY_TYP_NEXT											1
#define PLAY_TYP_PREV											2

#define ADMIN_CARD_MODE_MAX_VOLUME						1 // can't be 0 based due to voiceMenu
#define ADMIN_CARD_MODE_LOCK									2
#define ADMIN_CARD_MODE_MAX_PLAYTIME					3
#define ADMIN_CARD_MODE_SHUTDOWN_AFTER_TRACK 	4

extern uint8_t gpio_state;
extern PN532_I2C pn532i2c;
extern PN532 nfc;
extern settings mySettings;
extern list cardList;
extern listElement* card_scanned;
extern listElement* card_found;
extern DFMiniMp3 mp3;
extern PubSubClient client;
extern uint8_t uid[10];
extern uint8_t state;

#endif
