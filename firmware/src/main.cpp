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

bool voiceMenu(uint16_t* data, uint8_t max, uint16_t offset, uint8_t* status, bool preview, int previewFromFolder){
	bool ret=false;
	bool play=false;
	if(gpio_pushed&(1<<MCP_PIN_UP)){
		*status = 1;
		if(*data<max){
			*data=(*data+1);
		} else {
			*data=(max);
		}
		ret = true; // return true and play
		play = true; // key down so we should at least play the last track again
	}
	if(gpio_pushed&(1<<MCP_PIN_DOWN)){
		*status = 2;
		if(*data>1){
			*data=(*data-1);
		} else {
			*data=(1);
		}
		ret = true; // return true and play
		play = true;
	}
	if(gpio_pushed&(1<<MCP_PIN_PLAY)){
		*status = 3;
		if(*data>max){
			*data=(max);
		} else if(*data<1){
			*data=(1);
		}
		return true; // return directly true without play
	}

	if(play){
		if(preview){
			mp3.playFolderTrack(previewFromFolder, *data);
		} else {
			Serial.printf("Spiele File %i\r\n",offset + *data);
			mp3.playMp3FolderTrack(offset + *data);
		}
		return ret;
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
	if (card_found == NULL || card_found->get_mode() == 0 || card_found->get_mode() > 5 || card_found->get_folder()==0 || card_found->get_track()==0) {
		return;
	}
	if(state>STATE_UNKNOWN_CARD_MODE){
		// don't play anything if we're in setup mode of a new card
		return;
	}
	// else .. calculate the "next" Track
	bool playNext = false;
	uint16_t numTracksInFolder = mp3.getFolderTrackCount(card_found->get_folder());
	Serial.print(F("Next Track: "));
	if (card_found->get_mode() == 1) {
		Serial.println(F("Hörspielmodus ist aktiv -> keinen neuen Track spielen"));
	}
	// Album Modus: kompletten Ordner spielen
	else if (card_found->get_mode() == 2) {
		Serial.print(F("Album Modus -> "));
		if(card_found->get_track() < numTracksInFolder){
			Serial.println(F("last track of folder."));
		} else {
			card_found->set_track(card_found->get_track()+1);
			Serial.print(F("playing Folder "));
			Serial.printf("%i, Track %i/%i\r\n",card_found->get_folder(),card_found->get_track(),numTracksInFolder);
			playNext = true;
		}
	}
	// Party Modus: Ordner in zufälliger Reihenfolge
	else if (card_found->get_mode() == 3) {
		card_found->set_track(random(1, numTracksInFolder + 1));
		Serial.println(
			F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
			playNext = true;
		}
		else if (card_found->get_mode() == 4) {
			Serial.println(F("Einzel Modus aktiv -> keinen neuen Track spielen"));
		}
		// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
		else if (card_found->get_mode() == 5) {
			//card->set_track(EEPROM.read(card->get_folder())); // TODO save status
			Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
			"Fortschritt merken"));
			playNext = true;
		}
		// and fire
		if(playNext){
			mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
		}
	}

	// will only be called if i hit the "prevTrack" key
	void prevTrack(){
		if (card_found == NULL || card_found->get_mode() == 0 || card_found->get_mode() > 5 || card_found->get_folder()==0 || card_found->get_track()==0) {
			return;
		}
		if (card_found->get_mode() == 1) {
			Serial.println(F("Hörspielmodus ist aktiv -> Track von vorne spielen"));
		}
		else if (card_found->get_mode() == 2) {
			Serial.println(F("Albummodus ist aktiv -> vorheriger Track"));
			card_found->set_track(_max(1,_max(card_found->get_track(),2)-1));
		}
		else if (card_found->get_mode() == 3) {
			Serial.println(F("Party Modus ist aktiv -> Track von vorne spielen"));
		}
		else if (card_found->get_mode() == 4) {
			Serial.println(F("Einzel Modus aktiv -> Track von vorne spielen"));
		}
		else if (card_found->get_mode() == 5) {
			Serial.println(F("Hörbuch Modus ist aktiv -> vorheriger Track und "
			"Fortschritt speichern"));
			card_found->set_track(_max(1,_max(card_found->get_track(),2)-1));
			// Fortschritt im EEPROM abspeichern
			// TODO: EEPROM.write(myCard.folder, currentTrack);
		}
		mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
	}


void setup(){
	// serial startup


	Serial.begin(115200);
	for (uint8_t i = 0; i < 10; i++) {
		Serial.printf("%i.. ",i);
		delay(500);
	}
	Serial.println("\r\n====== SETUP =======");
	Serial.println(F("TonESP 0.1 startup"));

	// Initialize I2C
	Wire.begin(SDA_PIN, SCL_PIN);

	cardList.load();
	// wifi
	//wifi_setup();
	//wifi_connected();

	state = STATE_IDLE;
	pinMode(MCP_IRQ_PIN, INPUT);
	attachInterrupt(MCP_IRQ_PIN, mcp_irq_callback, FALLING);


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


	// mcp - port expander
	Serial.println(F("mcp init"));
	if(mcp.begin()){
		Serial.println("mcp found");
	} else {
		Serial.println("mcp not found");
	}
	mcp.setupInterrupts(true, false, LOW);
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

	// mp3 init
	mp3.begin();
	// check connection
	mp3.setVolume(6);
	if(mp3.getVolume()==6){
		mp3.setVolume(4);
		if(mp3.getVolume()==4){
			Serial.println("Mp3 Player online");
			mp3.max_file_in_folder = mp3.getTotalFolderCount();
		} else {
			Serial.println("Mp3 Player offline");
		}
	} else {
		Serial.println("Mp3 Player offline");
	}

	//////////////////// NFC ///////////////
	nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
	while (! versiondata) {
	 	Serial.println("Didn't find PN53x board");
		nfc.begin();
		versiondata = nfc.getFirmwareVersion();
		// Set the max number of retry attempts to read from a card
		// This prevents us from waiting forever for a card, which is
		// the default behaviour of the PN532.
		nfc.setPassiveActivationRetries(0xFF);
		// configure board to read RFID tags
  	nfc.SAMConfig();
 	}
	Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
	memset(uid, 0xff, 10);
	card_scanned = new listElement(uid,10); // fill with fake data
	last_seen_card = millis(); // will trigger a "look for card"

	Serial.println("====== SETUP =======");
}


// main loop .. this should run very often, no delay no internal loop
void loop(){
	// if the mcp triggered an interrupt
	if (mcp_interrupt_pending) {
		mcp_interrupt_pending = false;
		Serial.println(F("mcp interrupt"));
		gpio_state = mcp.readGPIO(0); // port A (inputs)
		Serial.printf("new gpio state %x\r\n",gpio_state);
		//Serial.printf("Last interrupt cause was on pin %i with value %i\r\n",mcp.getLastInterruptPin(), mcp.getLastInterruptPinValue());
	}

	// evaluates keys without unsetting the pressed var,
	gpio_pushed = key_menu();

	// wifi checking
	//wifi_status
	//wifi_scan_connect();

	// feed mp3,this will only see if we've receivedd a complete status msg that we didn't request.
	// no actaul communication will bestarted on this behalf
	mp3.status = mp3.loop();
	if(mp3.status){
		Serial.print(F("Mp3 Feedback: "));
		if(mp3.status == DFMP3_PLAY_FINISHED){
			Serial.println(F("play finished"));
			//nextTrack(); not here, will be handled by busy pin going up
		} else if(mp3.status == DFMP3_CARD_ONLINE){
			Serial.println(F("card online"));
		} else if(mp3.status == DFMP3_CARD_INSERTED){
			Serial.println(F("card inserted"));
		} else if(mp3.status == DFMP3_CARD_REMOVED){
			Serial.println(F("card removed"));
		} else if(mp3.status == DFMP3_ERROR_GENERAL){
			Serial.println(F("general error"));
		} else {
			Serial.print(F("unknwon feedback: "));
			Serial.println(mp3.status);
		}
	}

	// check if the MFRC522 triggered the interrupt
	// bf 1011 1111
	if (!(gpio_state & (1 << MCP_PIN_MFRC522_IRQ))) { // low active interrupt
		Serial.println(F("RFID/NFC interrupt "));
		//delay(200);
		if(nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,50)){
			Serial.printf("found a card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i\r\n",uid[0],uid[1],uid[2],uid[3],uid[4],uid[5],uid[6],uid[7],uid[8],uid[9],uidLength);
			// card already there
			if(card_scanned->compare_uid(uid,uidLength)){
				Serial.println("same card");
				// don't handle card
			} else {
				card_scanned->set_uuid(uid, uidLength);
				Serial.println("new card");
				state = STATE_NEW_CARD;
			}
			last_seen_card = millis();
		}

	} else if(last_asked_for_card<last_seen_card && millis()>last_seen_card+1000){ // the +1000 limits the amount of requests to the i2c
		Serial.println("read Passive tag irq waiter reenabeld");
		uint8_t pn532_packetbuffer[3];
		pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
		pn532_packetbuffer[1] = 1;  // max 1 cards at once (we can set this to 2 later)
		pn532_packetbuffer[2] = PN532_MIFARE_ISO14443A;
		pn532i2c.writeCommand(pn532_packetbuffer, 3);
		last_asked_for_card = millis();
	}

	////// special button handling /////
	if(!(gpio_state&(1<<MCP_PIN_LEFT)) && !(gpio_state&(1<<MCP_PIN_RIGHT))){ // low active
		// left and right are pushed. lets find out for how long
		//Serial.println("both keys down"); // works
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
		if(state<STATE_UNKNOWN_CARD_MODE){
			nextTrack();
		}
	}

	///// pushing buttons /////
	if(gpio_pushed&(1<<MCP_PIN_RIGHT)){ // next
		Serial.println("Next Track");
		delay(200);
		nextTrack();
		Serial.println("Done");
		delay(200);
	}
	if(gpio_pushed&(1<<MCP_PIN_LEFT)){ // prev
		Serial.println("Prev Track");
		delay(200);
		prevTrack();
		Serial.println("Done");
		delay(200);
	}
	if(gpio_pushed&(1<<MCP_PIN_PLAY)){ // prev
		if(state<STATE_UNKNOWN_CARD_MODE){
			if(gpio_state&(1<<MCP_PIN_BUSY)){
				Serial.println("Play");
				delay(200);
				mp3.start();
			} else {
				Serial.println("Pause");
				delay(200);
				mp3.pause();
			}
		}
	}
	if(gpio_pushed&(1<<MCP_PIN_UP)){ // louder
		//uint8_t vol = mp3.getVolume();
		if(state<STATE_UNKNOWN_CARD_MODE){
			Serial.println("Increase volume");
			delay(200);
			mp3.increaseVolume();
			Serial.printf("Set to %i\r\n",mp3.getVolume());
			delay(200);
		}
	} else if(gpio_pushed&(1<<MCP_PIN_DOWN)){ // quiter
		//uint8_t vol = mp3.getVolume();
		if(state<STATE_UNKNOWN_CARD_MODE){
			Serial.println("Decrease volume");
			delay(200);
			mp3.decreaseVolume();
			Serial.printf("Set to %i\r\n",mp3.getVolume());
			delay(200);
		}
	}
	//max_volume

	///////////// STATE_NEW_CARD //////////////////
	if (state == STATE_NEW_CARD) {
		card_found = cardList.is_uid_known(card_scanned->get_uuid(), card_scanned->get_uuidLength());
		if (card_found != NULL) {
			Serial.println("Card is known to the database");
			// ADMIN CARD //
			if(card_found->get_mode() == MODE_ADMIN_CARD){
				Serial.println("admin card found");
				if(card_found->get_folder() == 1){ // Max volume

				} else if(card_found->get_folder() == 2){ // Max playtime

				}
			}
			// ADMIN CARD //
			// PLAY CARD //
			// card is known play track
			// check total folder count
			if (card_found->get_folder() > mp3.getTotalFolderCount()) {
				// TODO: play correct error Track
				mp3.playFolderTrack(1, 1);
			}
			// check max file in folder
			uint16_t numTracksInFolder = mp3.getFolderTrackCount(card_found->get_folder());
			// Hörspielmodus: eine zufällige Datei aus dem Ordner
			if (card_found->get_mode() == MODE_SINGLE_RANDOM_TRACK) {
				card_found->set_track(random(1, numTracksInFolder + 1));
				Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
				Serial.println(card_found->get_track());
			}
			// Album Modus: kompletten Ordner spielen
			else if (card_found->get_mode() == MODE_COMPLETE_FOLDER) {
				card_found->set_track(1);
				Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
			}
			// Party Modus: Ordner in zufälliger Reihenfolge
			else if (card_found->get_mode() == MODE_RANDOM_FOLDER) {
				card_found->set_track(random(1, numTracksInFolder + 1));
				Serial.println(
				  F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
			}
			// Einzel Modus: eine Datei aus dem Ordner abspielen
			else if (card_found->get_mode() == MODE_SINGLE_TRACK) {
				Serial.println(
				  F("Einzel Modus -> eine Datei aus dem Odrdner abspielen"));
			}
			// Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
			else if (card_found->get_mode() == MODE_COMPLETE_FOLDER_CONTINUUES) {
				//card_found->set_track(EEPROM.read(card_found->get_folder()));
				Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
				   "Fortschritt merken"));
			}
			// check and play
			if (card_found->get_track() > numTracksInFolder) {
				// TODO: play correct error Track
				mp3.playFolderTrack(1, 1);
			} else {
				mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
				state = STATE_REGULAR_PLAYBACK; // #State regular playing
			}
		} else {
			Serial.println("Card is unknwon, start setup");
			state = STATE_UNKNOWN_CARD_INTRO; // #State unknown card_found
			Serial.println("---------> State = STATE_UNKNOWN_CARD_INTRO");
			if(card_found){ // if a card_found was already constructed, destruct it upfront
				delete card_found; // delete, so we can create a new, clean card_found
			}
			card_found = new listElement(card_scanned->get_uuid(), card_scanned->get_uuidLength()); // create empty card_found
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
		Serial.println("---------> State = STATE_UNKNOWN_CARD_MODE");
	}
	else if(state == STATE_UNKNOWN_CARD_MODE){
		if(voiceMenu((uint16_t*)(card_found->get_mode_p()),6,310,&mp3_voice_menu_status)){
			if(mp3_voice_menu_status==3){ // play
				state = STATE_UNKNOWN_CARD_FOLDER;
				Serial.println("---------> State = STATE_UNKNOWN_CARD_FOLDER");
				mp3.playMp3FolderTrack(300);
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_FOLDER){
		if(voiceMenu((uint16_t*)(card_found->get_folder_p()),mp3.max_file_in_folder-2,0,&mp3_voice_menu_status)){
			if(mp3_voice_menu_status==3){ // play
				if(card_found->get_mode()==4){ // we only have to ask for the track in mode 4
					state = STATE_UNKNOWN_CARD_TRACK;
					Serial.println("---------> State = STATE_UNKNOWN_CARD_TRACK, spiele 320");
					mp3.max_file_in_folder = mp3.getFolderTrackCount(card_found->get_folder());
					mp3.playMp3FolderTrack(320);
				} else {
					state = STATE_UNKNOWN_CARD_STORE;
					Serial.println("---------> State = STATE_UNKNOWN_CARD_STORE");
				}
			}
		}
	}
	else if(state == STATE_UNKNOWN_CARD_TRACK){
		if(voiceMenu((uint16_t*)(card_found->get_track_p()),mp3.max_file_in_folder,0,&mp3_voice_menu_status)){
			if(mp3_voice_menu_status==3){ // play
				state = STATE_UNKNOWN_CARD_STORE;
				Serial.println("---------> State = STATE_UNKNOWN_CARD_STORE");
			} else {
				state = STATE_UNKNOWN_CARD_TRACK_POST_1;
				Serial.println("---------> State = STATE_UNKNOWN_CARD_TRACK_POST_1");
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
			delay(1000); /// grrr TODO sonst wird das "eins" nicht gespielt
			// -> low -> high transistion
			Serial.printf("Spiele folder %i und track %i",card_found->get_folder(), card_found->get_track());
			mp3.playFolderTrack(card_found->get_folder(), card_found->get_track());
			state = STATE_UNKNOWN_CARD_TRACK;
			Serial.println("---------> State = STATE_UNKNOWN_CARD_TRACK");
		}
	}
	else if(state == STATE_UNKNOWN_CARD_STORE){
		cardList.add_uid(card_found);
		if(cardList.store() && cardList.load()){
			mp3.playMp3FolderTrack(400); // OK
			delete card_found;
		} else {
			mp3.playMp3FolderTrack(401); // FAIL
		}
		//card_found = NULL; // assign to NULL to avoid delete
		state = STATE_IDLE; // or STATE_UNKNOWN_CARD_STORE_POST_1 wait for high, wait for low and play?
		Serial.println("---------> State = STATE_IDLE");
	}
////////////// setup card ///////////////
} // loop
