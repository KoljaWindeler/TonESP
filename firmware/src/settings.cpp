#include "settings.h"

settings::settings(){
	SPIFFS.begin();
	m_max_volume = 20;
	m_current_volume = 4;
	m_locked = 0;
}

settings::~settings(){
};

bool settings::load(){
	if(SPIFFS.exists("/settings.txt")){
		//Serial.println("File existed");
		File f = SPIFFS.open("/settings.txt", "r");
		uint8_t t;
		// read MAX volume
		if(f.available()){
			f.read(&t,1);
			set_max_volume(t);
		}
		// read current volume
		if(f.available()){
			f.read(&m_current_volume,1);
		};

		// read lock mode
		if(f.available()){
			f.read(&m_locked,1);
		};
		print_settings();
		return true;
	};
	return false;
};

void settings::print_settings(){
	Serial.println(F("==== Settings ======="));
	Serial.printf("Max Volume %i\r\n",m_max_volume);
	Serial.printf("Current volume %i\r\n",m_current_volume);
	Serial.printf("lock %i\r\n",m_locked);
	Serial.println(F("==== Settings End ==="));
}

bool settings::store(){
	Serial.println("storing /settings.txt");
	delay(200);
	File f = SPIFFS.open("/settings.txt", "w");
	if (!f) {
    Serial.println("file open failed");
		return false;
	} else {
		f.write(m_max_volume);
		f.write(m_current_volume);
		f.write(m_locked);
	}
	f.close();
	return true;
};


void settings::set_max_volume(uint8_t v){
		if(v>20){
			v=20; // or was it 32?
		}
		m_max_volume = v;
};


uint8_t settings::get_max_volume(){
	return m_max_volume;
};
