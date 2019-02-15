#include "main.h"


PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);
settings mySettings;

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

bool voiceMenu(uint8_t * data, uint8_t max, uint16_t offset, uint8_t * status){
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
		debug_printf("Menu",COLOR_YELLOW,"MP3 File %i\r\n", offset + *data);
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
	// shuldn't matter
	power_down_at_ts = 0;
	standby_at_ts = 0;
	//delay(16000);
}

// either the "next" track was pushed or the last track was over
void play(uint8_t typ){
	//reset_autoreset(); --> done via BUSY
	if(mySettings.m_locked){
		debug_println(("Play"),COLOR_RED,F("Play avoided, player in lock_mode"));
		return;
	}
	//// track error handling ////
	if (card_found == NULL){
		debug_println(("Play"),COLOR_RED,F("Track avoided card_found == NULL"));
		return;
	} else if(card_found->get_mode() == 0 || card_found->get_mode() > 5 || card_found->get_folder() == 0) {
		debug_printf(("Play"),COLOR_RED,"Track avoided Mode %i, Folder %i, Track %i\r\n",card_found->get_mode(), card_found->get_folder(), card_found->get_track());
		return;
	}
	// check max file in folder
	mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
	if (card_found->get_folder() > mp3.max_folder - 2) { // - 2 for advert and mp3 folder
		// TODO: play correct error Track
		debug_printf(("Play"),COLOR_RED,"Folder (%i) > available Folders on card (%i)\r\n",card_found->get_folder(),mp3.max_folder - 2);
		return;
	}
	//// track error handling ////

	if (state > STATE_UNKNOWN_CARD_MODE) {
		// don't play anything if we're in setup mode of a new card
		return;
	}
	debug_print(("Play"),COLOR_YELLOW,F(""));
	// else .. calculate the "next" Track
	bool playNext = true;
	mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
	if(typ==PLAY_TYP_NEXT){
		Serial.print(F("Next, "));
	} else if(typ==PLAY_TYP_PREV){
		Serial.print(F("Prev, "));
	} else {
		typ=PLAY_TYP_FIRST; // just to be sure
		Serial.print(F("Start, "));
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
		debug_printf("Play",COLOR_PURPLE,"Folder %02i, Track %03i\r\n",card_found->get_folder(), card_found->get_track());
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
	Serial.println(F("TonESP 0.3 startup"));

	pinMode(MCP_IRQ_PIN, INPUT);

	// load data from storage
	cardList.load(); // load cards from storage
	memset(uid, 0x01, 10); // 10 bytes of 0x01 == Startup card
	card_scanned  = new listElement(uid, 10); // create new card, just to be able to call the card functions
	card_found = cardList.is_uid_known(uid, 10); // search for emulated startup card
	//Serial.printf("card_scanned lieg bei adresse %i\r\n",card_scanned);
	//debug_printf("card_scanned lieg bei adresse %i\r\n",card_scanned);
	if(card_found != NULL){
		// startup card found, play
		debug_println(("card"),COLOR_GREEN,F("Startup card found"));
		state = STATE_NEW_CARD;
	} else {
		// if there is no startup card in storage
		debug_println(("card"),COLOR_YELLOW,F("No startup card found"));
		state = STATE_IDLE; // no startup card, so keep idle
	}

	// load settings
	mySettings.load();

	// wifi
	wifi_setup();
	wifi_scan_connect();
	randomSeed(millis()); // somewhat random because the connection take randm time

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
	debug_println(("mcp"),COLOR_YELLOW,F("search"));
	if (mcp.begin()) {
		debug_println(("mcp"),COLOR_GREEN,"found");
		attachInterrupt(MCP_IRQ_PIN, mcp_irq_callback, FALLING);
		mcp.setupInterrupts(true, false, LOW);
		for (uint8_t p = 0; p < 7; p++) {
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
			debug_println(("mp3"),COLOR_GREEN,"Player online");
			mp3.max_folder = mp3.getTotalFolderCount();
			mp3.setVolume(mySettings.m_current_volume);
		} else {
			debug_println(("mp3"),COLOR_RED,"Player offline");
		}
	} else {
		debug_println(("mp3"),COLOR_RED,"Player offline");
	}

	// ////////////////// NFC ///////////////
	nfc.begin();
	uint32_t versiondata = nfc.getFirmwareVersion();
	while (!versiondata) {
		debug_println(("PN532"),COLOR_RED,"Didn't find board");
		nfc.begin();
		versiondata = nfc.getFirmwareVersion();
		// Set the max number of retry attempts to read from a card
		// This prevents us from waiting forever for a card, which is
		// the default behaviour of the PN532.
	}
	nfc.setPassiveActivationRetries(0xFF);
	// configure board to read RFID tags
	nfc.SAMConfig();
	debug_print(("PN532"),COLOR_GREEN,"Found chip PN5");
	Serial.println((versiondata >> 24) & 0xFF, HEX);
	debug_print(("PN532"),COLOR_GREEN,"Firmware ver. ");
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
	wifi_scan_connect();

	// feed mp3,this will only see if we've receivedd a complete status msg that we didn't request.
	// no actaul communication will bestarted on this behalf
	mp3.status = mp3.loop();
	if (mp3.status) {
		debug_print(("mp3"),COLOR_PURPLE,F("Feedback: "));
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
		debug_print(("RFID/NFC"),COLOR_YELLOW,F("interrupt "));
		// delay(200);
		if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
			// card already there
			//Serial.printf("Scanned card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i \r\n", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uidLength);
			if (card_scanned->compare_uid(uid, uidLength)) {
				//Serial.printf("SAME card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i ", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uidLength);
				if(millis()-last_seen_card>3000){
					Serial.println("Card was absend for >3sec, restart play");
					state = STATE_NEW_CARD;
				}
				// don't handle card
			} else {
				card_scanned->set_uuid(uid, uidLength);
				//Serial.printf("NEW card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uidLength);
				state = STATE_NEW_CARD;
			}
			last_seen_card = millis();
		}
		Serial.println(F(""));
	} else if (last_asked_for_card < last_seen_card && millis() > last_seen_card + 1000) { // the +1000 limits the amount of requests to the i2c
		debug_println(("RFID/NFC"),COLOR_YELLOW,F("read Passive tag irq waiter re-enabeld"));
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

	if(standby_at_ts){
		if(millis() > standby_at_ts){
			if(!(gpio_state&(1<<MCP_PIN_BUSY))){
				// player is running
				standby_at_ts = 0;
			} else {
				powerDown(F("standby power down"));
			}
		}
	}

	// /// end of track handling /////
	if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
		//Serial.println("BUSY lifted");
		reset_autoreset();
	}

	// /// pushing buttons /////
	if (gpio_pushed & (1 << MCP_PIN_RIGHT)) { // next
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.println("Next Track");
			//delay(200);
			play(PLAY_TYP_NEXT);
			Serial.println("Done");
			//delay(200);
		}
	}
	if (gpio_pushed & (1 << MCP_PIN_LEFT)) { // prev
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.println("Prev Track");
			//delay(200);
			play(PLAY_TYP_PREV);
			Serial.println("Done");
			//delay(200);
		}
	}
	if (gpio_pushed & (1 << MCP_PIN_PLAY)) { // play/pause
		if (state < STATE_UNKNOWN_CARD_MODE) {
			if(mySettings.m_locked){
				debug_println(("Play"),COLOR_RED,F("Play avoided, player in lock_mode"));
			} else {
				if (gpio_state & (1 << MCP_PIN_BUSY)) {
					debug_println("Play",COLOR_PURPLE,"Play");
					//delay(200);
					mp3.start();
					// hmm what about the device should be locked ?
				} else {
					debug_println("Play",COLOR_PURPLE,"Pause");
					//delay(200);
					mp3.pause();
				}
			}
		}
	}

	// Louder
	if (gpio_pushed & (1 << MCP_PIN_UP)) {
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.print("Increase volume, ");
			//delay(200);
			if(mySettings.m_current_volume < mySettings.get_max_volume()){
				mp3.increaseVolume();
				mySettings.m_current_volume = mp3.getVolume(); // works without stopping the audio
				mySettings.store();
			}
			Serial.printf("Volume set to %i\r\n", mySettings.m_current_volume);
		}
	} else if (gpio_pushed & (1 << MCP_PIN_DOWN)) { // quiter
		if (state < STATE_UNKNOWN_CARD_MODE) {
			Serial.print("Decrease volume, ");
			if(mySettings.m_current_volume > 1){
				mp3.decreaseVolume();
				mySettings.m_current_volume = mp3.getVolume(); // works without stopping the audio
				mySettings.store();
			}
			Serial.printf("Volume set to %i\r\n", mySettings.m_current_volume);
		}
	}

	// /////////// STATE_NEW_CARD //////////////////
	if (state == STATE_NEW_CARD) {
		card_found = cardList.is_uid_known(card_scanned->get_uuid(), card_scanned->get_uuidLength());
		if (card_found != NULL) {
			publish_card(card_found);
			debug_println(("card"),COLOR_GREEN,F("known to the database"));
			//////////////////////////////////// ADMIN CARD ////////////////////////////////////
			if (card_found->get_mode() == MODE_ADMIN_CARD) {
				debug_print(("card"),COLOR_YELLOW,F("admin card found "));
				// Max volume
				if (card_found->get_folder() == ADMIN_CARD_MODE_MAX_VOLUME) {
					mySettings.set_max_volume(card_found->get_track());
					Serial.printf("Set max volume to %i\r\n", mySettings.get_max_volume());
				}
				// lock mode
				else if (card_found->get_folder() == ADMIN_CARD_MODE_LOCK) {
					if(mySettings.m_locked){
						mySettings.m_locked = 0;
						Serial.println(F("Device unlocked"));
					} else {
						mySettings.m_locked = 1;
						Serial.println(F("Device locked"));
					}
					mySettings.store();
					// card consumed
					//delete card_found; -> don't delete, it is a known card
					//card_found=NULL;
					state = STATE_IDLE;
				}
				// Max playtime
				else if (card_found->get_folder() == ADMIN_CARD_MODE_MAX_PLAYTIME) {
					power_down_at_ts = millis()+card_found->get_track()*60000UL; // track in minutes
					Serial.printf("shutting down in %i minutes\r\n",card_found->get_track());
					state = STATE_IDLE;
				}
				// shut down after track
				else if (card_found->get_folder() == ADMIN_CARD_MODE_SHUTDOWN_AFTER_TRACK) {
					Serial.println(F("shutting down after track"));
					power_down_after_track = true;
					state = STATE_IDLE;
				}
			}
			//////////////////////////////////// ADMIN CARD ////////////////////////////////////
			//////////////////////////////////// PLAY CARD ////////////////////////////////////
			else {
				// card is known play track
				if(!mySettings.m_locked){
					play(PLAY_TYP_FIRST);
					state = STATE_REGULAR_PLAYBACK; // #State regular playing
				} else {
					debug_println(("play"),COLOR_RED,F("Device in lock mode, not playing"));
					// card consumed
					state = STATE_IDLE;
				}
			}
			// PLAY CARD //
		} else {
			debug_println(("card"),COLOR_RED,F("is unknwon, start setup"));
			state = STATE_UNKNOWN_CARD_INTRO; // #State unknown card_found
			debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_INTRO"));
			if(card_found==NULL && card_found->is_temporary){
				delete card_found;
			}
			card_found = new listElement(card_scanned->get_uuid(), card_scanned->get_uuidLength()); // create empty card_found
			publish_card(card_found);
		}
	}
	// /////////// STATE_NEW_CARD //////////////////
	// ///// playback ////////////////
	else if (state == STATE_REGULAR_PLAYBACK) {
		// hmm
	}
	// ///// playback ////////////////
	// //////////// setup card ///////////////
	else if (state == STATE_UNKNOWN_CARD_INTRO) {
		mp3.playMp3FolderTrack(310);
		state = STATE_UNKNOWN_CARD_MODE;
		debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_MODE"));
	} else if (state == STATE_UNKNOWN_CARD_MODE)    {
		if (voiceMenu(card_found->get_mode_p(), 6, 310, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				if(card_found->get_mode()==MODE_ADMIN_CARD){
					// admin menu
					state = STATE_UNKNOWN_CARD_ADMIN_MODE;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_ADMIN_1"));
					mp3.playMp3FolderTrack(300); // TODO ??
				} else {
					state = STATE_UNKNOWN_CARD_FOLDER;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_FOLDER"));
					mp3.playMp3FolderTrack(300);
				}
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_ADMIN_MODE) {
		if (voiceMenu(card_found->get_folder_p(), 6, 0, &mp3_voice_menu_status)) { // 6 ? 0 is certainly wrong
			if (mp3_voice_menu_status == 3) { // play
				if(card_found->get_folder() == ADMIN_CARD_MODE_MAX_VOLUME){
					state = STATE_UNKNOWN_CARD_ADMIN_VALUE;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_ADMIN_VALUE"));
					mp3.playMp3FolderTrack(300); // TODO "select value"
				} else if(card_found->get_folder() == ADMIN_CARD_MODE_MAX_PLAYTIME){
					state = STATE_UNKNOWN_CARD_ADMIN_VALUE;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_ADMIN_VALUE"));
					mp3.playMp3FolderTrack(300); // TODO "select value"
				} else {
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_STORE"));
					state = STATE_UNKNOWN_CARD_STORE;
				}
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_ADMIN_VALUE) {
		if (voiceMenu(card_found->get_track_p(), 60, 0, &mp3_voice_menu_status)) { // 6 ? 0 is certainly wrong
			if (mp3_voice_menu_status == 3) { // play
				debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_STORE"));
				state = STATE_UNKNOWN_CARD_STORE;
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_FOLDER)    {
		if (voiceMenu(card_found->get_folder_p(), mp3.max_folder - 2, 0, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				if (card_found->get_mode() == 4) { // we only have to ask for the track in mode 4
					state = STATE_UNKNOWN_CARD_TRACK;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_TRACK, spiele 320"));
					mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
					mp3.playMp3FolderTrack(320);
				} else {
					state = STATE_UNKNOWN_CARD_STORE;
					debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_STORE"));
				}
			} else {
				state = STATE_UNKNOWN_CARD_FOLDER_POST_1;
				debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_TRACK_POST_1"));
				delay(400); // needed, mp3 player command needs time to receive cmd.
				// before this the busy pin isn't up, so the pushed is over
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_FOLDER_POST_1)    {
		// at this point we're waiting for the player to finish playing the "four" or whatever number we're in
		// player is low while playing
		if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
			// -> low -> high transistion
			//Serial.printf("Spiele folder %i und track %i", card_found->get_folder(), 1);
			mp3.playFolderTrack(card_found->get_folder(), 1);
			state = STATE_UNKNOWN_CARD_FOLDER;
			debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_FOLDER"));
		}
	} else if (state == STATE_UNKNOWN_CARD_TRACK)    {
		if (voiceMenu(card_found->get_track_p(), mp3.max_file_in_folder, 0, &mp3_voice_menu_status)) {
			if (mp3_voice_menu_status == 3) { // play
				state = STATE_UNKNOWN_CARD_STORE;
				debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_STORE"));
			} else { // up / down
				state = STATE_UNKNOWN_CARD_TRACK_POST_1;
				debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_TRACK_POST_1"));
				delay(400);
				// we reach this only if up or down were pressed
				// we could change into some sort of STATE_UNKNOWN_CARD_TRACK_POST_1 state
				// STATE_UNKNOWN_CARD_TRACK_POST_1 would check the busy signal (should go high t play "eins" r whatever)
			}
		}
	} else if (state == STATE_UNKNOWN_CARD_TRACK_POST_1)    {
		// at this point we're waiting for the player to finish playing the "four" or whatever number we're in
		// player is low while playing
		if (gpio_pushed & (1 << MCP_PIN_BUSY)) {
			// -> low -> high transistion
			//Serial.printf("Spiele folder %i und track %i", card_found->get_folder(), card_found->get_track());
			mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
			state = STATE_UNKNOWN_CARD_TRACK;
			debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_UNKNOWN_CARD_TRACK"));
		}
	} else if (state == STATE_UNKNOWN_CARD_STORE)    {
		cardList.add_uid(card_found);
		if (cardList.store() && cardList.load()) {
			mp3.playMp3FolderTrack(400); // OK
			// delete card_found; ist schon durch die load schleife geschehen, create new temp card like done in setup
			memset(uid, 0xff, 10);
			card_scanned   = new listElement(uid, 10);
			card_found = NULL;
		} else {
			mp3.playMp3FolderTrack(401); // FAIL
		}
		state = STATE_IDLE; // or STATE_UNKNOWN_CARD_STORE_POST_1 wait for high, wait for low and play?
		debug_println(("STATE"),COLOR_YELLOW,F("-> STATE_IDLE"));
		delay(200);
	}
	// //////////// setup card ///////////////
} // loop


void reset_autoreset(){
	if(!(gpio_state&(1<<MCP_PIN_BUSY))){
		// player is running
		standby_at_ts = 0;
	} else {
		standby_at_ts = millis() + 10*60*1000; // 10 min
		//standby_at_ts = millis() + 60*1000;
		//Serial.printf("Setting timer to shut down in 1 min\r\n");
	}
}
