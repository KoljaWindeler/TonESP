#include "main.h"


// The int handler will just signal that the int has happen
// we will do the work from the main loop.
void mcp_irq_callback(){
	mcp_interrupt_pending = true;
}

void setup(){
	Serial.begin(115200);
	for (uint8_t i = 0; i < 10; i++) {
		Serial.printf("%i.. ",i);
		delay(500);
	}
	Serial.println(F("\r\nTonESP 0.1 startup"));

	state = STATE_IDLE;
	card = new listElement(mfrc522.uid.uidByte,10); // trash data, but at least we get a clean card
	pinMode(MCP_IRQ_PIN, INPUT);
	attachInterrupt(MCP_IRQ_PIN, mcp_irq_callback, FALLING);

	Wire.begin(SDA_PIN, SCL_PIN); // Initialize I2C

	if(0){
		Serial.println("Scanning i2c bus...");
		for (uint8_t address = 1; address < 127; address++){
			Wire.beginTransmission(address);
			if(Wire.endTransmission()==0){
				Serial.printf("Device at 0x%02x\r\n", address);
			}
		}
		Serial.println("Scan done");
	}


	Serial.println(F("mcp init"));
	if(mcp.begin()){
		Serial.println("mcp found");
	} else {
		Serial.println("mcp not found");
	}

	// We mirror INTA and INTB, so that only one line is required between MCP and Arduino for int reporting
	// The INTA/B will not be Floating
	// INTs will be signaled with a LOW
	mcp.setupInterrupts(true, false, LOW);

	// configuration for all buttons on port A (0-7)
	for (uint8_t p = 0; p < 8; p++) {
		mcp.pinMode(p, INPUT);
		mcp.pullUp(p, HIGH); // turn on a 100K pullup internally
		mcp.setupInterruptPin(p, CHANGE);

		gpio_first_pushed[p]=0;
	}

	// // Port B (8-15)
	// rfid reset
	mcp.pinMode(MCP_PIN_MFRC522_RESET, OUTPUT);
	mcp.digitalWrite(MCP_PIN_MFRC522_RESET, HIGH); // disable reset
	// power switch
	mcp.pinMode(MCP_PIN_POWER_SWITCH, OUTPUT);
	mcp.digitalWrite(MCP_PIN_POWER_SWITCH, HIGH); // activate power


	// Init MFRC522 with a hardware reset
	mcp.digitalWrite(MCP_PIN_MFRC522_RESET, LOW);
	delay(50);
	mcp.digitalWrite(MCP_PIN_MFRC522_RESET, HIGH);
	//Serial.println(F("mfrc522 init"));
	//delay(1000);
	//mfrc522.PCD_Init();
	//ShowReaderDetails(); // Show details of PCD - MFRC522 Card Reader details
	//Serial.println(F("Scan PICC to see UID, type, and data blocks..."));

	//mp3.begin();
	//mp3.setVolume(6);
}

// so we got different variables
// 1. gpio_state -> holds the current state of the inputs
// 2. gpio_down -> is high (bitwise) if the key was down once or is. sort of temporary var
// 3. gpio_pushed -> is high (bitwise) if the key was down and is lifted now. this will only be high for one cycle
// 4. gpio_first_pushed -> array holding the timestamp of the first push
uint8_t key_menu(){
	uint8_t ret = 0;
	for(uint8_t k=0;k<8;k++){
		if(!(gpio_state & (1<<k)) && !(gpio_down & (1<<k))){ // pin is LOW and wasn't LOW before
			gpio_down |= 1<<k; // a bit strange: set "down" to 1 if the state is low (all inputs are actually low active)
			gpio_first_pushed[k] = millis();
		} else if((gpio_state & (1<<k)) && (gpio_down & (1<<k))){ // pin is HIGH and it was down before
			gpio_down &= ~(1<<k); // reset
			ret |= 1<<k; // add to the return
			Serial.printf("key Menu ret %i\r\n",ret);
		}
	}
	return ret;
}

bool voiceMenu(uint16_t in, uint8_t max, uint16_t offset, uint16_t* out, bool preview = false, int previewFromFolder = 0){
	bool ret=false;
	if(gpio_pushed&(1<<MCP_PIN_UP)){
		*out = _min(max,in+1);
		ret = true; // return true and play
	}
	if(gpio_pushed&(1<<MCP_PIN_DOWN)){
		*out = _max(1,_max(in,2)-1); // max(1,max(0,2)-1) = max(1,2-1) = 1, max(1,max(2,2)-1) = max(1,2-1) = 1
		ret = true; // return true and play
	}
	if(gpio_pushed&(1<<MCP_PIN_PLAY)){
		return true; // return directly true without play
	}

	if(ret){
		if(preview){
			mp3.playFolderTrack(previewFromFolder, *out);
		} else {
			mp3.playMp3FolderTrack(offset + *out);
		}
		return true;
	}
	return false;
}

// we're shutting down on multiple occasios
void powerDown(const __FlashStringHelper* log){
	Serial.println(log);
	delay(200);
	mcp.digitalWrite(MCP_PIN_POWER_SWITCH, LOW);
	delay(16000);
}

// either the "next" track was pushed or the last track was over
void nextTrack(){
	if(state>STATE_UNKNOWN_CARD_MODE){
		// don't play anything if we're in setup mode of a new card
		return;
	}
	// else .. calculate the "next" Track
	bool playNext = false;
	uint16_t numTracksInFolder = mp3.getFolderTrackCount(card->get_folder());
	Serial.print(F("Next Track: "));
	if (card->get_mode() == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> keinen neuen Track spielen"));
	}
	// Album Modus: kompletten Ordner spielen
	else if (card->get_mode() == 2) {
		Serial.print(F("Album Modus -> "));
		if(card->get_track() < numTracksInFolder){
			Serial.println(F("last track of folder."));
		} else {
			card->set_track(card->get_track()+1);
			Serial.print(F("playing Folder "));
			Serial.printf("%i, Track %i/%i\r\n",card->get_folder(),card->get_track(),numTracksInFolder);
			playNext = true;
		}
	}
	// Party Modus: Ordner in zufälliger Reihenfolge
	else if (card->get_mode() == 3) {
		card->set_track(random(1, numTracksInFolder + 1));
		Serial.println(
			F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
		playNext = true;
	}
	else if (card->get_mode() == 4) {
		Serial.println(F("Einzel Modus aktiv -> keinen neuen Track spielen"));
	}
	// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
	else if (card->get_mode() == 5) {
		//card->set_track(EEPROM.read(card->get_folder())); // TODO save status
		Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
			 "Fortschritt merken"));
		playNext = true;
	}
	// and fire
	if(playNext){
		mp3.playFolderTrack(card->get_folder(), card->get_track());
	}
}

// will only be called if i hit the "prevTrack" key
void prevTrack(){
	if (card->get_mode() == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> Track von vorne spielen"));
  }
  else if (card->get_mode() == 2) {
    Serial.println(F("Albummodus ist aktiv -> vorheriger Track"));
    card->set_track(_max(1,_max(card->get_track(),2)-1));
  }
  else if (card->get_mode() == 3) {
    Serial.println(F("Party Modus ist aktiv -> Track von vorne spielen"));
  }
  else if (card->get_mode() == 4) {
    Serial.println(F("Einzel Modus aktiv -> Track von vorne spielen"));
  }
  else if (card->get_mode() == 5) {
    Serial.println(F("Hörbuch Modus ist aktiv -> vorheriger Track und "
                     "Fortschritt speichern"));
    card->set_track(_max(1,_max(card->get_track(),2)-1));
    // Fortschritt im EEPROM abspeichern
    // TODO: EEPROM.write(myCard.folder, currentTrack);
  }
	mp3.playFolderTrack(card->get_folder(), card->get_track());
}

// main loop .. this should run very often, no delay no internal loop
void loop(){
	// if the mcp triggered an interrupt
	if (mcp_interrupt_pending) {
		mcp_interrupt_pending = false;
		Serial.println(F("mcp interrupt"));
		gpio_state = mcp.readGPIO(0); // port A (inputs)
		Serial.printf("new gpio state %x\r\n",gpio_state);
	}

	// evaluates keys without unsetting the pressed var,
	gpio_pushed = key_menu();

	// check if the MFRC522 triggered the interrupt
	if (!(gpio_state & (1 << MCP_PIN_MFRC522_IRQ))) { // TODO: high active?
		if (mfrc522.PICC_IsNewCardPresent()) {
			if (mfrc522.PICC_ReadCardSerial()) {
				mfrc522.PICC_DumpToSerial(&(mfrc522.uid));
				state = STATE_NEW_CARD; // #NewCard State
			}
		}
	}

	////// special button handling /////
	if(!(gpio_state&(1<<MCP_PIN_LEFT)) && !(gpio_state&(1<<MCP_PIN_RIGHT))){ // low active
		// left and right are pushed. lets find out for how long
		Serial.println("both keys down");
		if(gpio_first_pushed[MCP_PIN_LEFT] && millis()-gpio_first_pushed[MCP_PIN_LEFT]>1000){
			if(gpio_first_pushed[MCP_PIN_RIGHT] && millis()-gpio_first_pushed[MCP_PIN_RIGHT]>1000){
				// power down
				powerDown(F("Left and Right, hold each for >1 sec. Shutting down"));
			}
		}
	}
	////// special button handling /////

	/// power down Timer
	if(power_down_at_ts){
		if(millis()>power_down_at_ts){
			powerDown(F("timer power down"));
		}
	}

	///// end of track handling /////
	if(gpio_pushed&(1<<MCP_PIN_BUSY)){
		// power down
		if(power_down_after_track){
			powerDown(F("Power down after track. Shutting down"));
		}
		// busy was DOWN (playing) and is UP (not longer playing)
		nextTrack();
	}

	///// pushing buttons /////
	if(gpio_pushed&(1<<MCP_PIN_RIGHT)){ // next
		nextTrack();
	}
	if(gpio_pushed&(1<<MCP_PIN_LEFT)){ // prev
		prevTrack();
	}
	if(gpio_pushed&(1<<MCP_PIN_UP)){ // louder
		//uint8_t vol = mp3.getVolume();
		if(state>STATE_UNKNOWN_CARD_MODE){
			mp3.increaseVolume();
		}
	} else if(gpio_pushed&(1<<MCP_PIN_DOWN)){ // quiter
		//uint8_t vol = mp3.getVolume();
		if(state>STATE_UNKNOWN_CARD_MODE){
			mp3.decreaseVolume();
		}
	}
	//max_volume

	///////////// STATE_NEW_CARD //////////////////
	if (state == STATE_NEW_CARD) {
		card = cardList.is_uid_known(mfrc522.uid.uidByte, mfrc522.uid.size);
		if (card != NULL) {
			// ADMIN CARD //
			if(card->get_mode() == MODE_ADMIN_CARD){
				if(card->get_folder() == 1){ // Max volume

				} else if(card->get_folder() == 2){ // Max playtime

				}
			}
			// ADMIN CARD //
			// PLAY CARD //
			// card is known play track
			// check total folder count
			if (card->get_folder() > mp3.getTotalFolderCount()) {
				// TODO: play correct error Track
				mp3.playFolderTrack(1, 1);
			}
			// check max file in folder
			uint16_t numTracksInFolder = mp3.getFolderTrackCount(card->get_folder());
			// Hörspielmodus: eine zufällige Datei aus dem Ordner
			if (card->get_mode() == MODE_SINGLE_RANDOM_TRACK) {
				card->set_track(random(1, numTracksInFolder + 1));
				Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
				Serial.println(card->get_track());
			}
			// Album Modus: kompletten Ordner spielen
			else if (card->get_mode() == MODE_COMPLETE_FOLDER) {
				card->set_track(1);
				Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
			}
			// Party Modus: Ordner in zufälliger Reihenfolge
			else if (card->get_mode() == MODE_RANDOM_FOLDER) {
				card->set_track(random(1, numTracksInFolder + 1));
				Serial.println(
				  F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
			}
			// Einzel Modus: eine Datei aus dem Ordner abspielen
			else if (card->get_mode() == MODE_SINGLE_TRACK) {
				Serial.println(
				  F("Einzel Modus -> eine Datei aus dem Odrdner abspielen"));
			}
			// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
			else if (card->get_mode() == MODE_COMPLETE_FOLDER_CONTINUUES) {
				//card->set_track(EEPROM.read(card->get_folder()));
				Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
				   "Fortschritt merken"));
			}
			// check and play
			if (card->get_track() > numTracksInFolder) {
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
			card = new listElement(mfrc522.uid.uidByte, mfrc522.uid.size); // create empty card
		}
	}
	///////////// STATE_NEW_CARD //////////////////

	/////// playback ////////////////
	else if(state == STATE_REGULAR_PLAYBACK){
		// next and prev button
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
					state = STATE_UNKNOWN_CARD_STORE;
				}
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_TRACK){
		if(voiceMenu(card->get_track(),mp3.getFolderTrackCount(card->get_folder()),0,&temp_16)){
			if(temp_16==card->get_track()){
				state = STATE_UNKNOWN_CARD_STORE;
			} else {
				state = STATE_UNKNOWN_CARD_TRACK_POST_1;
				// we reach this only if up or down were pressed
				// we could change into some sort of STATE_UNKNOWN_CARD_TRACK_POST_1 state
				// STATE_UNKNOWN_CARD_TRACK_POST_1 would check the busy signal (should go high t play "eins" r whatever)
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_TRACK_POST_1){
		// at this point we're waiting for the player to finish playing the "four" or whatever number we're in
		// player is low while playing
		if(gpio_pushed&(1<<MCP_PIN_BUSY)){
			// -> low -> high transistion
			mp3.playFolderTrack(card->get_folder(), card->get_track());
			state = STATE_UNKNOWN_CARD_TRACK;
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
