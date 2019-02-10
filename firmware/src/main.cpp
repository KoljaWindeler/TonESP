#include "main.h"

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// The int handler will just signal that the int has happen
// we will do the work from the main loop.
void mcp_irq_callback(){
	mcp_interrupt_pending = true;
}

// so we got different variables
// 1. gpio_state -> holds the current state of the inputs
// 2. gpio_down -> is high (bitwise) if the key was down once or is. sort of temporary var
// 3. gpio_pushed -> is high (bitwise) if the key was down and is lifted now. this will only be high for one cycle
// 4. gpio_first_pushed -> array holding the timestamp of the first push
uint8_t key_menu(){
	uint8_t ret = 0;

	for (uint8_t k = 0; k < 8; k++) {
		if (!(gpio_state & (1 << k)) && !(gpio_down & (1 << k))) { // pin is LOW and wasn't LOW before
			gpio_down |= 1 << k; // a bit strange: set "down" to 1 if the state is low (all inputs are actually low active)
			gpio_first_pushed[k] = millis();
		} else if ((gpio_state & (1 << k)) && (gpio_down & (1 << k))) { // pin is HIGH and it was down before
			gpio_down &= ~(1 << k); // reset
			ret       |= 1 << k;    // add to the return
			//Serial.printf("key Menu ret %i\r\n", ret);
		}
	}
	return ret;
}

bool voiceMenu(uint16_t * data, uint8_t max, uint16_t offset, uint8_t * status){
	bool ret  = false;
	bool play = false;
	//Serial.printf("in %i, max %i\r\n",*data,max);
	//if(*data>max){
	//	delay(10000);
	//}
	if (gpio_pushed & (1 << MCP_PIN_UP)) {
		*status = 1;
		//Serial.printf("in %i, max %i\r\n",*data,max);
		if (*data < max) {
			*data = (*data + 1);
		} else {
			*data = (max);
		}
		ret  = true; // return true and play
		play = true; // key down so we should at least play the last track again
	}
	if (gpio_pushed & (1 << MCP_PIN_DOWN)) {
		*status = 2;
		if (*data > 1 && *data<=max) {
			*data = (*data - 1);
		} else {
			*data = (1);
			//Serial.println("----> set to 1");
		}
		ret  = true; // return true and play
		play = true;
	}
	if (gpio_pushed & (1 << MCP_PIN_PLAY)) {
		*status = 3;
		if (*data > max) {
			*data = (max);
		} else if (*data < 1) {
			*data = (1);
		}
		return true; // return directly true without play
	}

	if (play) {
		Serial.printf("[Menu] MP3 File %i\r\n", offset + *data);
		mp3.playMp3FolderTrack(offset + *data);
		return ret;
	}
	return false;
} // voiceMenu

// we're shutting down on multiple occasios
void powerDown(const __FlashStringHelper * log){
	Serial.println(log);
	delay(200);
	mcp.digitalWrite(MCP_PIN_POWER_SWITCH, LOW);
	delay(16000);
}

// either the "next" track was pushed or the last track was over
void play(uint8_t typ){
	Serial.print(F("[Play] "));
	//// track error handling ////
	if (card_found == NULL){
		Serial.println(F("Track avoided card_found == NULL"));
		return;
	} else if(card_found->get_mode() == 0 || card_found->get_mode() > 5 || card_found->get_folder() == 0) {
		Serial.printf("Track avoided Mode %i, Folder %i, Track %i\r\n",card_found->get_mode(), card_found->get_folder(), card_found->get_track());
		return;
	}
	if (card_found->get_folder() > mp3.max_folder - 2) { // - 2 for advert and mp3 folder
		// TODO: play correct error Track
		Serial.println(F("Folder > available Folders on card"));
		return;
	}
	// check max file in folder
	mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
	if(card_found->get_track()>mp3.max_file_in_folder){
		Serial.println(F("Track > available Tracks in folder"));
		return;
	}
	//// track error handling ////

	if (state > STATE_UNKNOWN_CARD_MODE) {
		// don't play anything if we're in setup mode of a new card
		return;
	}
	// else .. calculate the "next" Track
	bool playNext = true;
	mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
	if(typ==PLAY_TYP_NEXT){
		Serial.print(F("Next, "));
	} else if(typ==PLAY_TYP_PREV){
		Serial.print(F("Prev, "));
	} else {
		typ=PLAY_TYP_FIRST; // just to be sure
		Serial.print(F("First, "));
	}

	if (card_found->get_mode() == MODE_SINGLE_RANDOM_TRACK) {
		Serial.print(F("Hörspielmodus, "));
		if(typ==PLAY_TYP_FIRST){
			card_found->set_track(random(1, mp3.max_file_in_folder + 1));
			Serial.println(F("zufälligen Track wiedergeben"));
		} else if(typ==PLAY_TYP_PREV){
			Serial.println(F("Track von vorne spielen"));
		} else if(typ==PLAY_TYP_NEXT){
			Serial.println(F("keinen neuen Track spielen"));
			playNext = false;
		}
	}

	// Album Modus: kompletten Ordner spielen
	else if (card_found->get_mode() == MODE_COMPLETE_FOLDER) {
		Serial.print(F("Album Modus, "));
		if(typ==PLAY_TYP_FIRST){
			card_found->set_track(1);
			Serial.println(F("Ordner von vorne wiedergeben"));
		} else if(typ==PLAY_TYP_PREV){
			Serial.println(F("vorheriger Track"));
			card_found->set_track(_max(1, _max(card_found->get_track(), 2) - 1));
		} else if(typ==PLAY_TYP_NEXT){
			if (card_found->get_track() >= mp3.max_file_in_folder) {
				Serial.println(F("last track of folder."));
				playNext = false;
			} else {
				card_found->set_track(card_found->get_track() + 1);
				Serial.println(F("next Track"));
			}
		}
	}

	// Party Modus: Ordner in zufälliger Reihenfolge
	else if (card_found->get_mode() == MODE_RANDOM_FOLDER) {
		Serial.print(F("Party Modus, "));
		if(typ==PLAY_TYP_PREV){
			Serial.println(F("Track von vorne spielen"));
		}	else {
			card_found->set_track(random(1, mp3.max_file_in_folder + 1));
			Serial.println(F("Ordner in zufälliger Reihenfolge wiedergeben"));
		}
	}

	// Einzel Modus: eine Datei aus dem Ordner abspielen
	else if (card_found->get_mode() == MODE_SINGLE_TRACK)   {
		Serial.print(F("Einzel Modus aktiv, "));
		if(typ==PLAY_TYP_FIRST){
			Serial.println(F("eine Datei aus dem Odrdner abspielen"));
		} else if(typ==PLAY_TYP_PREV){
			Serial.println(F("Track von vorne spielen"));
		} else if(typ==PLAY_TYP_NEXT){
			Serial.println(F("keinen neuen Track spielen"));
			playNext = false;
		}
	}
	// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
	else if (card_found->get_mode() == MODE_COMPLETE_FOLDER_CONTINUUES) {
		Serial.print(F("Hörbuch Modus, "));
		if(typ==PLAY_TYP_FIRST){
			// track is loaded from card
			Serial.println(F("kompletten Ordner spielen mit Fortschritt"));
		} else if(typ==PLAY_TYP_PREV){
			Serial.println(F("vorheriger Track und Fortschritt speichern"));
			card_found->set_track(_max(1, _max(card_found->get_track(), 2) - 1));
			// Fortschritt im SPIFFS abspeichern
			cardList.store();
		} else if(typ==PLAY_TYP_NEXT){
			Serial.println(F("kompletten Ordner spielen und Fortschritt merken"));
			if (card_found->get_track() >= mp3.max_file_in_folder) {
				Serial.println(F("last track of folder."));
				playNext = false;
				card_found->set_track(1);
			} else {
				card_found->set_track(card_found->get_track() + 1);
				Serial.println(F("next Track"));
			}
			// Fortschritt im SPIFFS abspeichern
			cardList.store();
		}
	}
	// and fire
	if (playNext) {
		Serial.printf("[Play] Folder %02i, Track %03i\r\n",card_found->get_folder(), card_found->get_track());
		mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
	}
} // play



void setup(){
	// serial startup
	Serial.begin(115200);
	for (uint8_t i = 0; i < 10; i++) {
		Serial.printf("%i.. ", i);
		delay(100);
	}
	Serial.println("\r\n====== SETUP =======");
	Serial.println(F("TonESP 0.1 startup"));


	state = STATE_IDLE;
	pinMode(MCP_IRQ_PIN, INPUT);
	cardList.load();
	memset(uid, 0xff, 10);
	card_scanned   = new listElement(uid, 10); // fill with fake data, just to be able to call the card functions

	// wifi
	// wifi_setup();
	// wifi_connected();

	// Initialize I2C
	Wire.begin(SDA_PIN, SCL_PIN);

	if (0) {
		Serial.println("Scanning i2c bus...");
		for (uint8_t address = 1; address < 127; address++) {
			Wire.beginTransmission(address);
			if (Wire.endTransmission() == 0) {
				Serial.printf("Device at 0x%02x\r\n", address);
			}
		}
		Serial.println("Scan done");
	}


	// ////////////////// mcp - port expander //////////////////
	Serial.println(F("mcp init"));
	if (mcp.begin()) {
		Serial.println("mcp found");
		attachInterrupt(MCP_IRQ_PIN, mcp_irq_callback, FALLING);
		mcp.setupInterrupts(true, false, LOW);
		for (uint8_t p = 0; p < 8; p++) {
			mcp.pinMode(p, INPUT);
			mcp.pullUp(p, HIGH); // turn on a 100K pullup internally
			mcp.setupInterruptPin(p, CHANGE);

			gpio_first_pushed[p] = 0;
		}

		mcp.pinMode(MCP_PIN_PN532_RESET, OUTPUT);
		mcp.digitalWrite(MCP_PIN_PN532_RESET, HIGH); // disable reset
		// power switch
		mcp.pinMode(MCP_PIN_POWER_SWITCH, OUTPUT);
		mcp.digitalWrite(MCP_PIN_POWER_SWITCH, HIGH); // activate power
		// Init PN532 with a hardware reset
		mcp.digitalWrite(MCP_PIN_PN532_RESET, LOW);
		delay(50);
		mcp.digitalWrite(MCP_PIN_PN532_RESET, HIGH);

	} else {
		Serial.println("!!!!!!!!!!!!!!!!!!!!!! mcp not found !!!!!!!!!!!!!!!!!!!!!!");
	}
	// // Port B (8-15)
	// rfid reset


	// ////////////////// mp3 init //////////////////
	mp3.begin();
	// check connection
	mp3.setVolume(6);
	if (mp3.getVolume() == 6) {
		mp3.setVolume(4);
		if (mp3.getVolume() == 4) {
			Serial.println("Mp3 Player online");
			mp3.max_folder = mp3.getTotalFolderCount();
		} else {
			Serial.println("Mp3 Player offline");
		}
	} else {
		Serial.println("Mp3 Player offline");
	}

	// ////////////////// NFC ///////////////
	nfc.begin();
	uint32_t versiondata = nfc.getFirmwareVersion();
	while (!versiondata) {
		Serial.println("Didn't find PN53x board");
		nfc.begin();
		versiondata = nfc.getFirmwareVersion();
		// Set the max number of retry attempts to read from a card
		// This prevents us from waiting forever for a card, which is
		// the default behaviour of the PN532.
	}
	nfc.setPassiveActivationRetries(0xFF);
	// configure board to read RFID tags
	nfc.SAMConfig();
	Serial.print("Found chip PN5");
	Serial.println((versiondata >> 24) & 0xFF, HEX);
	Serial.print("Firmware ver. ");
	Serial.print((versiondata >> 16) & 0xFF, DEC);
	Serial.print('.');
	Serial.println((versiondata >> 8) & 0xFF, DEC);
	last_seen_card = millis();                 // will trigger a "look for card"

	Serial.println("====== SETUP =======");
} // setup


////////////////////////////////// main loop .. this should run very often, no delay no internal loop ///////////////////////////
void loop(){
	// if the mcp triggered an interrupt
	if (mcp_interrupt_pending) {
		mcp_interrupt_pending = false;
		//Serial.println(F("mcp interrupt"));
		gpio_state = mcp.readGPIO(0); // port A (inputs)
		//Serial.printf("new gpio state %x\r\n", gpio_state);
		// Serial.printf("Last interrupt cause was on pin %i with value %i\r\n",mcp.getLastInterruptPin(), mcp.getLastInterruptPinValue());
	}

	// evaluates keys without unsetting the pressed var,
	gpio_pushed = key_menu();

	// wifi checking
	// wifi_status
	// wifi_scan_connect();

	// feed mp3,this will only see if we've receivedd a complete status msg that we didn't request.
	// no actaul communication will bestarted on this behalf
	mp3.status = mp3.loop();
	if (mp3.status) {
		Serial.print(F("[Mp3] Feedback: "));
		if (mp3.status == DFMP3_PLAY_FINISHED) {
			Serial.println(F("play finished"));
			if (power_down_after_track) {
				powerDown(F("Power down after track. Shutting down"));
			}
			// busy was DOWN (playing) and is UP (not longer playing)
			if (state < STATE_UNKNOWN_CARD_MODE) {
				play(PLAY_TYP_NEXT);
			}
		} else if (mp3.status == DFMP3_CARD_ONLINE) {
			Serial.println(F("card online"));
			mp3.max_folder = mp3.getTotalFolderCount();
		} else if (mp3.status == DFMP3_CARD_INSERTED) {
			Serial.println(F("card inserted"));
		} else if (mp3.status == DFMP3_CARD_REMOVED) {
			Serial.println(F("card removed"));
		} else if (mp3.status == DFMP3_ERROR_GENERAL) {
			Serial.println(F("general error"));
		} else {
			Serial.print(F("unknwon feedback: "));
			Serial.println(mp3.status);
		}
	}

	// check if the PN532 triggered the interrupt
	// bf 1011 1111
	if (!(gpio_state & (1 << MCP_PIN_PN532_IRQ))) { // low active interrupt
		Serial.print(F("[RFID/NFC] interrupt "));
		// delay(200);
		if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
			// card already there
			if (card_scanned->compare_uid(uid, uidLength)) {
				Serial.printf("SAME card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i ", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uidLength);
				if(millis()-last_seen_card>3000){
					Serial.println("Card was absend for >3sec, restart play");
					state = STATE_NEW_CARD;
				}
				// don't handle card
			} else {
				card_scanned->set_uuid(uid, uidLength);
				Serial.printf("NEW card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i -> ", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uidLength);
				state = STATE_NEW_CARD;
			}
			last_seen_card = millis();
		}
		Serial.println(F(""));
	} else if (last_asked_for_card < last_seen_card && millis() > last_seen_card + 1000) { // the +1000 limits the amount of requests to the i2c
		Serial.println(F("[Card] read Passive tag irq waiter re-enabeld"));
		uint8_t pn532_packetbuffer[3];
		pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
		pn532_packetbuffer[1] = 1; // max 1 cards at once (we can set this to 2 later)
		pn532_packetbuffer[2] = PN532_MIFARE_ISO14443A;
		pn532i2c.writeCommand(pn532_packetbuffer, 3);
		last_asked_for_card = millis();
	}

	// //// special button handling /////
	if (!(gpio_state & (1 << MCP_PIN_LEFT)) && !(gpio_state & (1 << MCP_PIN_RIGHT))) { // low active
		// left and right are pushed. lets find out for how long
		// Serial.println("both keys down"); // works
		if (gpio_first_pushed[MCP_PIN_LEFT] && millis() - gpio_first_pushed[MCP_PIN_LEFT] > 1000) {
			if (gpio_first_pushed[MCP_PIN_RIGHT] && millis() - gpio_first_pushed[MCP_PIN_RIGHT] > 1000) {
				// power down
				powerDown(F("Left and Right, hold each for >1 sec. Shutting down"));
			}
		}
	}
	// //// special button handling /////

	// / power down Timer
	if (power_down_at_ts) {
		if (millis() > power_down_at_ts) {
			powerDown(F("timer power down"));
		}
	}

	// /// end of track handling /////
	if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
		//Serial.println("BUSY lifted");
	}

	// /// pushing buttons /////
	if (gpio_pushed & (1 << MCP_PIN_RIGHT)) { // next
		Serial.println("Next Track");
		//delay(200);
		play(PLAY_TYP_NEXT);
		Serial.println("Done");
		//delay(200);
	}
	if (gpio_pushed & (1 << MCP_PIN_LEFT)) { // prev
		Serial.println("Prev Track");
		//delay(200);
		play(PLAY_TYP_PREV);
		Serial.println("Done");
		//delay(200);
	}
	if (gpio_pushed & (1 << MCP_PIN_PLAY)) { // prev
		if (state < STATE_UNKNOWN_CARD_MODE) {
			if (gpio_state & (1 << MCP_PIN_BUSY)) {
				Serial.println("Play");
				//delay(200);
				mp3.start();
			} else {
				Serial.println("Pause");
				//delay(200);
				mp3.pause();
			}
		}
	}
	if (gpio_pushed & (1 << MCP_PIN_UP)) { // louder
		// uint8_t vol = mp3.getVolume();
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.println("Increase volume");
			//delay(200);
			mp3.increaseVolume();
			Serial.printf("Set to %i\r\n", mp3.getVolume());
			//delay(200);
		}
	} else if (gpio_pushed & (1 << MCP_PIN_DOWN)) { // quiter
		// uint8_t vol = mp3.getVolume();
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.println("Decrease volume");
			//delay(200);
			mp3.decreaseVolume();
			Serial.printf("Set to %i\r\n", mp3.getVolume());
			//delay(200);
		}
	}
	// max_volume

	// /////////// STATE_NEW_CARD //////////////////
	if (state == STATE_NEW_CARD) {
		card_found = cardList.is_uid_known(card_scanned->get_uuid(), card_scanned->get_uuidLength());
		if (card_found != NULL) {
			Serial.println(F("[Card] known to the database"));
			// ADMIN CARD //
			if (card_found->get_mode() == MODE_ADMIN_CARD) {
				Serial.println(F("[Card] admin card found"));
				if (card_found->get_folder() == 1) { // Max volume
				} else if (card_found->get_folder() == 2) { // Max playtime
				}
			}
			// ADMIN CARD //
			// PLAY CARD //
			else {
				// card is known play track
				play(PLAY_TYP_FIRST);
				state = STATE_REGULAR_PLAYBACK; // #State regular playing
			}
			// PLAY CARD //
		} else {
			Serial.println(F("[Card] is unknwon, start setup"));
			state = STATE_UNKNOWN_CARD_INTRO; // #State unknown card_found
			Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_INTRO"));
			if (card_found) { // if a card_found was already constructed, destruct it upfront
				delete card_found; // delete, so we can create a new, clean card_found
			}
			card_found = new listElement(card_scanned->get_uuid(), card_scanned->get_uuidLength()); // create empty card_found
		}
	}
	// /////////// STATE_NEW_CARD //////////////////
	// ///// playback ////////////////
	else if (state == STATE_REGULAR_PLAYBACK) {
		// next and prev button
	}
	// ///// playback ////////////////
	// //////////// setup card ///////////////
	else if (state == STATE_UNKNOWN_CARD_INTRO) {
		mp3.playMp3FolderTrack(310);
		state = STATE_UNKNOWN_CARD_MODE;
		Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_MODE"));
	} else if (state == STATE_UNKNOWN_CARD_MODE)    {
		if (voiceMenu((uint16_t *) (card_found->get_mode_p()), 6, 310, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				state = STATE_UNKNOWN_CARD_FOLDER;
				Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_FOLDER"));
				mp3.playMp3FolderTrack(300);
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_FOLDER)    {
		if (voiceMenu((uint16_t *) (card_found->get_folder_p()), mp3.max_folder - 2, 0, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				if (card_found->get_mode() == 4) { // we only have to ask for the track in mode 4
					state = STATE_UNKNOWN_CARD_TRACK;
					Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_TRACK, spiele 320"));
					mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
					mp3.playMp3FolderTrack(320);
				} else {
					state = STATE_UNKNOWN_CARD_STORE;
					Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_STORE"));
				}
			} else {
				state = STATE_UNKNOWN_CARD_FOLDER_POST_1;
				Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_TRACK_POST_1"));
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_FOLDER_POST_1)    {
		// at this point we're waiting for the player to finish playing the "four" or whatever number we're in
		// player is low while playing
		if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
			delay(1000); // / grrr TODO sonst wird das "eins" nicht gespielt, wahrschienlch kann man auch ein kleineres delay vor dem post status setzen nutzen?
			// -> low -> high transistion
			Serial.printf("Spiele folder %i und track %i", card_found->get_folder(), 1);
			mp3.playFolderTrack(card_found->get_folder(), 1);
			state = STATE_UNKNOWN_CARD_FOLDER;
			Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_FOLDER"));
		}
	} else if (state == STATE_UNKNOWN_CARD_TRACK)    {
		if (voiceMenu((uint16_t *) (card_found->get_track_p()), mp3.max_file_in_folder, 0, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				state = STATE_UNKNOWN_CARD_STORE;
				Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_STORE"));
			} else { // up / down
				state = STATE_UNKNOWN_CARD_TRACK_POST_1;
				Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_TRACK_POST_1"));
				// we reach this only if up or down were pressed
				// we could change into some sort of STATE_UNKNOWN_CARD_TRACK_POST_1 state
				// STATE_UNKNOWN_CARD_TRACK_POST_1 would check the busy signal (should go high t play "eins" r whatever)
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_TRACK_POST_1)    {
		// at this point we're waiting for the player to finish playing the "four" or whatever number we're in
		// player is low while playing
		if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
			delay(1000); // / grrr TODO sonst wird das "eins" nicht gespielt
			// -> low -> high transistion
			Serial.printf("Spiele folder %i und track %i", card_found->get_folder(), card_found->get_track());
			mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
			state = STATE_UNKNOWN_CARD_TRACK;
			Serial.println(F("[STATE] -> STATE_UNKNOWN_CARD_TRACK"));
		}
	} else if (state == STATE_UNKNOWN_CARD_STORE)    {
		cardList.add_uid(card_found);
		if (cardList.store() && cardList.load()) {
			mp3.playMp3FolderTrack(400); // OK
			// delete card_found; ist schon durch die load schleife geschehen
			memset(uid, 0xff, 10);
			card_scanned   = new listElement(uid, 10);
			card_found = NULL;
		} else {
			mp3.playMp3FolderTrack(401); // FAIL
		}
		state = STATE_IDLE; // or STATE_UNKNOWN_CARD_STORE_POST_1 wait for high, wait for low and play?
		Serial.println(F("[STATE] -> STATE_IDLE"));
		delay(200);
	}
	// //////////// setup card ///////////////
} // loop
