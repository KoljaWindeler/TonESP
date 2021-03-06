#include "WiFi.h"
#include "globals.h"


const int maxSize   = 20;
uint8_t wifi_status = WIFI_STATE_DISCONNECTED;
char wifi_config[6][maxSize];
WiFiClient espClient;
PubSubClient client(espClient);
char m_topic_buffer[TOPIC_BUFFER_SIZE];
char m_msg_buffer[MSG_BUFFER_SIZE];
uint32_t wifi_last_connect = 0;


bool publish_online(char *t){
	uint8_t buffer[64];
	uint8_t ret;
	ret = client.publish(build_topic("online", UNIT_TO_PC), t);
	for (uint8_t i = 0; i < 10; i++) {
		client.loop();
		delay(10); // let wifi process data
	}
	return ret;
}

bool publish_lock(){
	uint8_t buffer[64];
	uint8_t ret;
	if(mySettings.m_locked){
		ret = client.publish(build_topic("lock", UNIT_TO_PC), (char *) "ON");
	} else {
		ret = client.publish(build_topic("lock", UNIT_TO_PC), (char *) "OFF");
	}
	for (uint8_t i = 0; i < 10; i++) {
		client.loop();
		delay(10); // let wifi process data
	}
	return ret;
}

bool publish_shutdown_afterTrack(){
	uint8_t buffer[64];
	uint8_t ret;
	if(power_down_after_track){
		ret = client.publish(build_topic("shutdownTrack", UNIT_TO_PC), (char *) "ON");
	} else {
		ret = client.publish(build_topic("shutdownTrack", UNIT_TO_PC), (char *) "OFF");
	}
	for (uint8_t i = 0; i < 10; i++) {
		client.loop();
		delay(10); // let wifi process data
	}
	return ret;
}


bool publish_card(listElement * e, uint8_t level){
	uint8_t buffer[64];
	uint8_t ret;

	if (e == NULL) {
		return false;
	}
	if(level==0){
		ret = client.publish(build_topic("card", UNIT_TO_PC), "OFF");
	}
	for (uint8_t i = 0; i < 10; i++) {
		client.loop();
		delay(10); // let wifi process data
	}
	e->dump_ascii(buffer,level);
	for (uint8_t i = 0; i < 10; i++) {
		client.loop();
		delay(10); // let wifi process data
	}
	ret = client.publish(build_topic("card", UNIT_TO_PC), (char *) buffer);
	return ret;
}

// call via MQTT callback
// IN Topic, payload and length
void callback(char * p_topic, byte * p_payload, uint16_t p_length){
	p_payload[p_length] = 0x00;
	debug_printf("MQTT", 0, "Topic %s, Msg %s\r\n", p_topic, p_payload);
	reset_autoreset();
	// /////////// TOPIC: ctrl /////////////
	if (strcmp((const char *) p_topic, build_topic("ctrl", PC_TO_UNIT)) == 0) {
		if (strcmp((const char *) p_payload, (const char *) "pause") == 0) {
			if (gpio_state & (1 << MCP_PIN_BUSY)) {
				Serial.println("Play");
				// delay(200);
				mp3.start();
				// hmm what about the device should be locked ?
			} else {
				Serial.println("Pause");
				// delay(200);
				mp3.pause();
			}
		} else if (!strcmp((const char *) p_payload, (const char *) "next"))        {
			play(PLAY_TYP_NEXT);
		} else if (!strcmp((const char *) p_payload, (const char *) "prev"))        {
			play(PLAY_TYP_PREV);
		} else if (!strcmp((const char *) p_payload, (const char *) "backup"))        {
			// reuse card scanned, so we can keep card_found as is
			uint16_t ii = 0;
			card_scanned = cardList.get_element_nr(ii);
			// uint8_t buffer[32];
			while (card_scanned != NULL) {
				publish_card(card_scanned,3); //3 == more info
				ii++;
				card_scanned = cardList.get_element_nr(ii);
			}
		}
		// wifi update
		else if (!strncmp((const char *) p_payload, "http", 4)) { // update
			ESPhttpUpdate.rebootOnUpdate(false);
			HTTPUpdateResult res = ESPhttpUpdate.update(p_payload);
			if (res == HTTP_UPDATE_OK) {
				client.publish(build_topic("out", UNIT_TO_PC), (char *) "rebooting...");
				debug_println("Wifi", COLOR_GREEN, "Update ok, reboot");
				delay(200);
				ESP.restart();
			} else   {
				debug_println("Wifi", COLOR_RED, "Update failed");
			}
		}
	}
	// /////////// TPOIC: card /////////////
	else if (strcmp((const char *) p_topic, build_topic("card", PC_TO_UNIT)) == 0) {
		// Format of the payload is [action],[uid],[mode],[folder],[track]
		// e.g. 1,01010101010101010101,3,2,1 -> create or modify card with uid 10x01 should be set to mode 3, folder 2, track 1
		// actially supported are: [create/modify],[delete],[format],[emulate]
		// [format] is dangerous and will delete the complete storage. therefore all fields, track,mode,folder have to be set to 99
		p_payload[p_length]     = ',';  // easier to parse
		p_payload[p_length + 1] = 0x00; // not really impoortant
		p_length++;
		uint8_t section = 0; // action=0, uid=1, mode=2, folder=3, track=4
		uint8_t action  = 0; // add/modify, erase, format, emulate
		uint8_t uid_i   = 0; // byte for uid (ascii)
		uint16_t t      = 0; // 16 bit temp
		uint8_t data_ok = 0; // 1== all data for action received, execute
		uint8_t userdata[10];
		memset(userdata,0x00,10);
		for (uint8_t i = 0; i < p_length; i++) {
			if (p_payload[i] != ',') {
				// ascii to byte
				if (p_payload[i] >= 'A' && p_payload[i] <= 'Z') {
					p_payload[i] -= 'A' - 10;
				} else {
					p_payload[i] -= '0';
				}
				// // section 0 reads action ////
				if (section == 0) {
					action = p_payload[i];
					uid_i=0;
				}
				// // section 1 reads uid ////
				else if (section == 1) {
					if (uid_i % 2 == 0) { // high nibble, shift
						uid[uid_i / 2] = 0x00 | (p_payload[i] << 4);
					} else { // low nibble
						uid[uid_i / 2] |= p_payload[i];
					}
					if (uid_i < 19) { // limit uid_i. uid_i=20 -> uid[10] and valid is only uid[0..9]
						uid_i++;
					}
				}
				// // section 2,3,4 read numbers for track / folder / mode ////
				else if (section >= 2 && section<=4) {
					t = t * 10 + p_payload[i];
					uid_i=0; // reset for section 5
				}
				// section 5 is reading Userdata
				else if (section == 5) {
					if (uid_i % 2 == 0) { // high nibble, shift
						userdata[uid_i / 2] = 0x00 | (p_payload[i] << 4);
					} else { // low nibble
						userdata[uid_i / 2] |= p_payload[i];
					}
					if (uid_i < 19) { // limit uid_i. uid_i=20 -> userdata[10] and valid is only userdata[0..9]
						uid_i++;
					}
				}
			}
			// as soon as we parse a ',' act on the data. Depending on section
			else {
				// set action, all clear
				// if(section==0){
				// section 1 reads uid
				if (section == 1) {
					// Serial.printf("MQTT card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x %i \r\n", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uid_i/2);
					if (card_scanned == NULL) {
						Serial.println("!!! Critical Error, CARD_SCANNED was null. This is not supposed to happen !!!");
						card_scanned = new listElement(uid, (uid_i + 1) / 2); // create new card, just to be able to call the card functions
					} else {
						card_scanned->set_uuid(uid, (uid_i + 1) / 2);
					}
					// if we nly need the UUID -> end here
					if (action == ACTION_EMULATE || action == ACTION_DELETE) {
						data_ok = 1;
						break; // we don't need more, leav for loop here
					}
				}
				// section 2 reads mode
				else if (section == 2) {
					card_scanned->set_mode(t);
					t = 0;
				}
				// section 3 reads folder
				else if (section == 3) {
					card_scanned->set_folder(t);
					t = 0;
				}
				// section 4 reads track. This is the last section, so perform acton after this
				else if (section == 4) {
					card_scanned->set_track(t);
					// format needs mode, album & track info
					if (action == ACTION_FORMAT) {
						data_ok = 1;
						break;
					}
				}
				// section 5, reading userdata done
				else if (section == 5) {
					//for(uint i=0; i<10; i++){
					//	Serial.printf("%02X",userdata[i]);
					//}
					card_scanned->set_userdata(userdata);
					// format needs mode, album & track info
					if (action == ACTION_CREATE) {
						data_ok = 1;
						break;
					}
				}
				// end of sections
				section++;
			} // char is ','
		}  // for loop

		// execute
		if (data_ok) {
			// find card, only needed for create and deleted
			// not needed for format or emulate
			if (action == ACTION_CREATE || action == ACTION_DELETE) {
				card_found = cardList.is_uid_known(card_scanned->get_uuid(), card_scanned->get_uuidLength());
				t = 0;
				if (card_found != NULL) {
					debug_println("Card", COLOR_GREEN, F("known to the database"));
					t = 1;
				} else {
					debug_println("Card", COLOR_GREEN, F("is unknown, creating temporary"));
					card_found = new listElement(card_scanned->get_uuid(), card_scanned->get_uuidLength()); // create empty card_found
				}
			}

			// ACTION 1 = Add/Change card
			if (action == ACTION_CREATE) {
				// char buffer[50];
				// card_scanned->dump_ascii((uint8_t*)buffer);
				// Serial.println(buffer);
				card_found->set_mode(card_scanned->get_mode());
				card_found->set_folder(card_scanned->get_folder());
				card_found->set_track(card_scanned->get_track());
				card_found->set_userdata(card_scanned->get_userdata());
				if (!t) {
					// Serial.println(F("Adding card "));
					cardList.add_uid(card_found);
				} else {
					// Serial.println(F("Modifying card "));
				}
				// uint16_t i = card_found and restore
				cardList.store();
				cardList.load();
				// load already erased card_found
			}
			// ACTION 2 = Remove card
			else if (action == ACTION_DELETE) {
				if (t) { // card was known to the database
					debug_println("Card", COLOR_GREEN, F("deleting card"));
					cardList.remove_uid(card_found);
					cardList.store();
					cardList.load();
					// load already erased card_found
				} else {
					delete card_found;
					card_found = NULL;
				}
			}
			// ACTION 3 = Format storage
			else if (action == ACTION_FORMAT) {
				Serial.print(F("Formating Storage "));
				if (card_scanned->get_track() == 99 && card_scanned->get_mode() == 99 && card_scanned->get_folder() == 99) {
					Serial.println(F("Code correct, executing"));
					SPIFFS.begin();
					SPIFFS.format();
					cardList.load();
				} else {
					Serial.println(F("wrong key, stopping"));
					delete card_found;
					card_found = NULL;
				}
			}
			// ACTION 4 = emulate card
			else if (action == ACTION_EMULATE) {
				debug_println("Card", COLOR_GREEN, F("emulating cards"));
				state = STATE_NEW_CARD;
			}
			return;
		} // data ok
		else {
			debug_println("WiFi", COLOR_RED, F("not enough data"));
		}
	} // topic == card
} // callback

void wifi_setup(){
	// WiFi.disconnect();
	// WiFi.mode(WIFI_STA);
	sprintf(wifi_config[0], WIFI_SSID_1);
	sprintf(wifi_config[1], WIFI_PW_1);
	sprintf(wifi_config[2], WIFI_SSID_2);
	sprintf(wifi_config[3], WIFI_PW_2);
	sprintf(wifi_config[4], WIFI_SSID_3);
	sprintf(wifi_config[5], WIFI_PW_3);
	// delay(100);
}

bool wifi_scan_connect(){
	if (!wifi_connected() && (wifi_status != WIFI_STATE_CONNECTING || (millis() - wifi_last_connect > 30000))) {
		// Serial.printf("wifi_status %i\r\n",wifi_status);
		WiFi.mode(WIFI_STA);
		debug_println("Wifi", COLOR_YELLOW, "Scanning...");
		int n = WiFi.scanNetworks();
		if (n > 0) {
			uint8_t best      = 0;
			int8_t best_value = WiFi.RSSI(0);
			for (int i = 0; i < n; i++) {
				// Serial.println(WiFi.SSID(i));
				// Serial.println(WiFi.RSSI(i));

				for (uint8_t ii = 0; ii < 3; ii++) {
					if (WiFi.SSID(i).compareTo(wifi_config[ii * 2])) {
						if (WiFi.RSSI(i) > best_value) {
							// Serial.print("Best WiFi updated to ");
							// Serial.println(i);
							best       = i;
							best_value = WiFi.RSSI(i);
						}
					}
				}
			}
			debug_printf("Wifi", COLOR_YELLOW, "Connect to %s -> %s\r\n", wifi_config[best * 2], wifi_config[best * 2 + 1]);
			WiFi.mode(WIFI_STA);
			WiFi.disconnect();
			delay(1);
			WiFi.begin(wifi_config[best * 2], wifi_config[best * 2 + 1]);
			wifi_last_connect = millis();
			wifi_status       = WIFI_STATE_CONNECTING; // connecting
			if (0) {
				while (WiFi.status() != WL_CONNECTED) {
					delay(500);
					Serial.print(".");
				}
			}
			// Serial.printf("wifi status %i\r\n",WiFi.status());
			// Serial.println(WiFi.localIP());
			// Serial.println(WiFi.status());
			wifi_connected();
		}
	}
} // wifi_scan_connect

bool wifi_connected(){
	if (WiFi.status() == WL_CONNECTED) {
		if (wifi_status == WIFI_STATE_DISCONNECTED || !client.connected()) {
			client.setServer(IPAddress(MQTT_SERVER_IP), MQTT_SERVER_PORT);
			client.setCallback(callback); // in main.cpp
			// Serial.println(WiFi.localIP());
			if (client.connect("TonESP1", MQTT_LOGIN, MQTT_PW)) {
				// if(client.connect("TonESP1", "ha", "ah", "TonESP1/INFO", 0, true, "lost signal")){
				debug_println("MQTT", COLOR_GREEN, "connected");
				if ((gpio_state & (1 << MCP_PIN_BUSY)) && !mySettings.m_locked) { // active high, play only if nothing else is on
					mp3.playAdvertisement(201);
				}
				wifi_status = WIFI_STATE_CONNECTED;

				client.subscribe(build_topic("ctrl", PC_TO_UNIT)); // MQTT_TRACE_TOPIC topic
				for (uint8_t i; i < 10; i++) {
					client.loop();
				}
				client.subscribe(build_topic("card", PC_TO_UNIT)); // MQTT_TRACE_TOPIC topic
				for (uint8_t i; i < 10; i++) {
					client.loop();
				}
				// only to show that we're online
				publish_online("ON");
			}
		}
		client.loop();
		return true;
	} else {
		if (wifi_status > WIFI_STATE_CONNECTING) {
			wifi_status = WIFI_STATE_DISCONNECTED;
			if ((gpio_state & (1 << MCP_PIN_BUSY)) && !mySettings.m_locked) { // active how
				mp3.playAdvertisement(202);
			}
		}
		return false;
	}
} // wifi_connected

char * build_topic(const char * topic, uint8_t pc_shall_R_or_S, bool with_dev){
	if (pc_shall_R_or_S != PC_TO_UNIT && pc_shall_R_or_S != UNIT_TO_PC) {
		pc_shall_R_or_S = PC_TO_UNIT;
	}
	if (with_dev) {
		sprintf(m_topic_buffer, "%s/%c/%s", "TonESP", pc_shall_R_or_S, topic);
	} else {
		sprintf(m_topic_buffer, "%c/%s", pc_shall_R_or_S, topic);
	}
	return m_topic_buffer;
}

char * build_topic(const char * topic, uint8_t pc_shall_R_or_S){
	return build_topic(topic, pc_shall_R_or_S, true);
}
