#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include "cardlist.h"


// defines here
#define MCP_IRQ_PIN 1 // TODO .. which ?
#define SDA_PIN     2
#define SCL_PIN     3

// port A - inputs
#define MCP_PIN_UP            0
#define MCP_PIN_DOWN          1
#define MCP_PIN_LEFT          2
#define MCP_PIN_RIGHT         3
#define MCP_PIN_PLAY          4
#define MCP_PIN_BUSY          5
#define MCP_PIN_MFRC522_IRQ   6
// port B - outputs
#define MCP_PIN_MFRC522_RESET 7
#define MCP_PIN_POWER_SWITCH  8

#define STATE_IDLE								0
#define STATE_NEW_CARD 						1
#define STATE_REGULAR_PLAYBACK 		2
#define STATE_UNKNOWN_CARD_INTRO 	3
#define STATE_UNKNOWN_CARD_MODE		100
#define STATE_UNKNOWN_CARD_FOLDER	110
#define STATE_UNKNOWN_CARD_TRACK	120
#define STATE_UNKNOWN_CARD_STORE	130

// declare vars here
uint16_t temp_16;
uint8_t gpio_state         = 0;
uint8_t gpio_pressed       = 0;
bool mcp_interrupt_pending = false;

Adafruit_MCP23017 mcp;
list cardList;
listElement* card;


void setup(){
	state = STATE_IDLE;
	pinMode(MCP_IRQ_PIN, INPUT);
	attachInterrupt(MCP_IRQ_PIN, mcp_irq_callback, FALLING);

	Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C
	mcp.begin();                  // use default address 0

	// We mirror INTA and INTB, so that only one line is required between MCP and Arduino for int reporting
	// The INTA/B will not be Floating
	// INTs will be signaled with a LOW
	mcp.setupInterrupts(true, false, LOW);

	// configuration for all buttons on port A (0-7)
	for (uint8_t p = 0; p < 8; p++) {
		mcp.pinMode(p, INPUT);
		mcp.pullUp(p, HIGH); // turn on a 100K pullup internally
		mcp.setupInterruptPin(p, FALLING);
	}

	// // Port B (8-15)
	// rfid reset
	mcp.pinMode(MCP_PIN_MFRC522_RESET, OUTPUT);
	mcp.pullUp(MCP_PIN_MFRC522_RESET, HIGH); // disable reset
	// power switch
	mcp.pinMode(MCP_PIN_POWER_SWITCH, OUTPUT);
	mcp.pullUp(MCP_PIN_POWER_SWITCH, LOW); // activate power


	mfrc522.PCD_Init();  // Init MFRC522
	ShowReaderDetails(); // Show details of PCD - MFRC522 Card Reader details
	Serial.println(F("Scan PICC to see UID, type, and data blocks..."));
}

// The int handler will just signal that the int has happen
// we will do the work from the main loop.
void mcp_irq_callback(){
	mcp_interrupt_pending = true;
}


bool voiceMenu(uint16_t in, uint8_t max, uint16_t offset, uint16_t* out, bool preview = false, int previewFromFolder = 0){
	bool ret=false;
	if(key_menu()&(1<<MCP_PIN_UP)){
		*out = min(max,in+1);
		ret = true; // return true and play
	}
	if(key_menu()&(1<<MCP_PIN_DOWN)){
		*out = max(1,max(in,2)-1); // max(1,max(0,2)-1) = max(1,2-1) = 1, max(1,max(2,2)-1) = max(1,2-1) = 1
		ret = true; // return true and play
	}
	if(key_menu()&(1<<MCP_PIN_PLAY)){
		return true; // return directly true without play
	}

	if(ret){
		if(preview){
			mp3.playFolderTrack(previewFromFolder, out);
		} else {
			mp3.playMp3FolderTrack(offset + out);
		}
		return true;
	}
	return false;
}

uint8_t key_menu(){
	uint8_t ret = 0;
	for(uint8_t k=0;k<8;k++){
		if(!(gpio_state & (1<<k))){ // UP pin pressed
			gpio_pressed |= 1<<k;
		} else if((gpio_state & (1<<MCP_PIN_UP)) && (gpio_pressed & (1<<k))){ // UP pin was pressed and is lifted now
			gpio_pressed &= ~(1<<k);
			ret |= 1<<k;
		}
	}
	return ret;
}
/**
 * main routine: sleep the arduino, and wake up on Interrups.
 * the LowPower library, or similar is required for sleeping, but sleep is simulated here.
 * It is actually posible to get the MCP to draw only 1uA while in standby as the datasheet claims,
 * however there is no stadndby mode. Its all down to seting up each pin in a way that current does not flow.
 * and you can wait for interrupts while waiting.
 */
void loop(){
	// if the mcp triggered an interrupt
	if (mcp_interrupt_pending) {
		gpio_state = mcp.readGPIO(0); // port A (inputs)
		mcp_interrupt_pending = false;
	}

	// check if the MFRC522 triggered the interrupt
	if (gpio_state & (1 << MCP_PIN_MFRC522_IRQ)) {
		if (mfrc522.PICC_IsNewCardPresent()) {
			if (mfrc522.PICC_ReadCardSerial()) {
				mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
				state = STATE_NEW_CARD; // #NewCard State
			}
		}
	}

	///////////// STATE_NEW_CARD //////////////////
	if (state == STATE_NEW_CARD) {
		card = cardList.is_uid_known(mfrc522.uid);
		if (card != NULL) {
			// card is known play track
			// check total folder count
			if (card->get_folder() > mp3.getTotalFolderCount()) {
				// TODO: play correct error Track
				mp3.playFolderTrack(1, 1);
			}
			// check max file in folder
			numTracksInFolder = mp3.getFolderTrackCount(card->get_folder());
			// Hörspielmodus: eine zufällige Datei aus dem Ordner
			if (card->get_mode() == 1) {
				card->set_track(random(1, numTracksInFolder + 1));
				Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
				Serial.println(currentTrack);
			}
			// Album Modus: kompletten Ordner spielen
			else if (card->get_mode() == 2) {
				card->set_track(1);
				Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
			}
			// Party Modus: Ordner in zufälliger Reihenfolge
			else if (card->get_mode() == 3) {
				card->set_track(random(1, numTracksInFolder + 1));
				Serial.println(
				  F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
			}
			// Einzel Modus: eine Datei aus dem Ordner abspielen
			else if (card->get_mode() == 4) {
				Serial.println(
				  F("Einzel Modus -> eine Datei aus dem Odrdner abspielen"));
			}
			// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
			else if (card->get_mode() == 5) {
				card->set_track(EEPROM.read(card->get_folder()));
				Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
				   "Fortschritt merken"));
			}
			// check and play
			if (currentTrack > numTracksInFolder) {
				// TODO: play correct error Track
				mp3.playFolderTrack(1, 1);
			} else {
				mp3.playFolderTrack(card->get_folder(), card->get_track());
				state = STATE_REGULAR_PLAYBACK; // #State regular playing
			}
		} else {
			state = STATE_UNKNOWN_CARD_INTRO; // #State unknown card
			if(card){ // if a card was already constructed, destruct it upfront
				delete card; // delete, so we can create a new, clean card
			}
			card = new listElement(mfrc522.uid); // create empty card
		}
	}
	///////////// STATE_NEW_CARD //////////////////

	/////// playback ////////////////
	else if(state == STATE_REGULAR_PLAYBACK){
		// next and prev
	}
	/////// playback ////////////////

	////////////// setup card ///////////////
	else if(state == STATE_UNKNOWN_CARD_INTRO){
		mp3.playMp3FolderTrack(310);
		state = STATE_UNKNOWN_CARD_MODE;
	}
	else if(state == STATE_UNKNOWN_CARD_MODE){
		if(voiceMenu(card->get_mode(),6,310,&temp_16)){
			if(temp_16==card->get_mode()){
				state = STATE_UNKNOWN_CARD_FOLDER;
				mp3.playMp3FolderTrack(300);
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_FOLDER){
		if(voiceMenu(card->get_folder(),99,0,&temp_16)){
			if(temp_16==card->get_folder()){
				if(card->get_mode()==4){ // we only have to ask for the track in mode 4
					state = STATE_UNKNOWN_CARD_TRACK;
					mp3.playMp3FolderTrack(320);
				} else {
					state = STATE_STORE_CARD;
				}
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_TRACK){
		if(voiceMenu(card->get_track(),mp3.getFolderTrackCount(card->get_folder()),0,&temp_16)){
			if(temp_16==card->get_track()){
				state = STATE_UNKNOWN_CARD_STORE;
			} else {
				// we reach this only if up or down were pressed
				// we could change into some sort of STATE_UNKNOWN_CARD_TRACK_POST_1 state
				// STATE_UNKNOWN_CARD_TRACK_POST_1 would check the busy signal (should go high t play "eins" r whatever)
				// and go to STATE_UNKNOWN_CARD_TRACK_POST_2. once busy goes low we could play the track
				// and come back to this state ...
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_STORE){
		cardList.add_uid(card);
		if(cardList.store()){
			mp3.playMp3FolderTrack(400); // OK
		} else {
			mp3.playMp3FolderTrack(401); // FAIL
		}
		card = NULL; // assign to NULL to avoid delete
		state = STATE_IDLE; // or STATE_UNKNOWN_CARD_STORE_POST_1 wait for high, wait for low and play?
	}
////////////// setup card ///////////////
} // loop
