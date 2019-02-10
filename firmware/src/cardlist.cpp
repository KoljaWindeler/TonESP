#include <cardlist.h>


listElement::listElement(byte* uid, uint8_t size){
	// copy uid
	set_uuid(uid,size);

	// set information
	m_next=NULL;
	m_folder=0;
	m_track=0;
	m_mode=0;
};

listElement::~listElement(){};

void listElement::set_uuid(byte* uid, uint8_t size){
	if(size>10){
		size = 10;
	}
	for(uint8_t i=0;i<size;i++){
		m_uidByte[i]=*uid;
		uid++;
	}
	for(uint8_t i=size; i<10;i++){
		m_uidByte[i]=0x00;
	}

	m_uidByteLength=size;
}

byte* listElement::get_uuid(){
	return m_uidByte;
}

uint8_t listElement::get_uuidLength(){
	return m_uidByteLength;
}


listElement* listElement::get_next(){
	return m_next;
}

bool listElement::compare_uid(byte* uid, uint8_t size){
	if(size>10){
		size = 10;
	}
	for(uint8_t i=0;i<size;i++){
		if(m_uidByte[i]!=*uid){
			return false;
		}
		uid++;
	}
	return true;
}

uint16_t listElement::get_mode(){
	return m_mode;
}

uint16_t* listElement::get_mode_p(){
	return &m_mode;
}

uint16_t listElement::get_folder(){
	return m_folder;
}

uint16_t* listElement::get_folder_p(){
	return &m_folder;
}

uint16_t listElement::get_track(){
	return m_track;
}

uint16_t* listElement::get_track_p(){
	return &m_track;
}

void listElement::set_mode(uint16_t mode){
	m_mode = mode;
}

void listElement::set_folder(uint16_t folder){
	m_folder = folder;
}

void listElement::set_track(uint16_t track){
	m_track = track;
}

void listElement::set_next(listElement* element){
	m_next = element;
}
//////////////////////

list::list(){
	length = 0;
	first = NULL;
	SPIFFS.begin();
	//SPIFFS.format();
};

list::~list(){};

bool list::store(){
	Serial.println("storing /cards.txt");
	delay(200);
	File f = SPIFFS.open("/cards.txt", "w");
	if (!f) {
    Serial.println("file open failed");
	} else {
		listElement* e = first;
		while(e!=NULL){
			//Serial.println("Storing 16 bytes");
			f.write(e->get_uuid(), 10);
			uint16_t t=e->get_mode();
			f.write(((uint8_t*)&t),2);
			t=e->get_folder();
			f.write(((uint8_t*)&t),2);
			t=e->get_track();
			f.write(((uint8_t*)&t),2);
			//Serial.printf("Mode: %i, Folder: %02i, Track: %03i\r\n",e->get_mode(), e->get_folder(), e->get_track());
			e = e->get_next();
		}
		f.close();
	}
	return true;
};

bool list::load(){
	uint16_t cards_loaded=0;

	uint8_t ii=0;
	while(first!=NULL){
		uint8_t i=0;
		listElement* e=first;
		listElement* e2=first;
		while(e2->get_next()!=NULL){
			e=e2;
			e2=e2->get_next();
			i++;
		}
		if(e2!=NULL){
			Serial.printf("Deleting card nr %i from RAM\r\n",i);
			delete e2;
			ii++;
		}
		if(i==0){
			first = NULL;
		} else {
			e->set_next(NULL);
		}
	}
	Serial.printf("%i cards deleted from RAM\r\n",ii);


	//Serial.println("loading /cards.txt");
	if(SPIFFS.exists("/cards.txt")){
		//Serial.println("File existed");
		File f = SPIFFS.open("/cards.txt", "r");
		byte uid[16];
		while(f.available()>=16){
			cards_loaded++;
			//Serial.println("Reading 16 bytes");
			//delay(200);
			f.read(uid, 16);
			listElement* e = new listElement(uid,10);
			//Serial.println("set mode");
			//delay(200);
			e->set_mode((((uint16_t)uid[11])<<4) | uid[10]);
			e->set_folder((((uint16_t)uid[13])<<4) | uid[12]);
			e->set_track((((uint16_t)uid[15])<<4) | uid[14]);
			//for(uint i=0; i<16; i++){
			//	Serial.printf("%02x ",uid[i]);
			//}
			//Serial.printf("\r\nMode: %i, Folder: %02i, Track: %03i\r\n",e->get_mode(), e->get_folder(), e->get_track());
			add_uid(e);
		}
		f.close();
		Serial.printf("%i cards loaded to RAM\r\n",cards_loaded);
	}
	return true;
};

void list::add_uid(listElement* element){
	if(first == NULL){
		//Serial.println("erstes element null, setze es");
		//delay(200);
		first = element;
		return;
	}
	listElement* e=first;
	while(e != NULL){
		if(e->get_next() == NULL){
			//Serial.println("e get next null, set next");
			//delay(200);
			e->set_next(element);
			length++;
			return;
		} else {
			//Serial.println("e get next not null");
			//delay(200);
			e = e->get_next();
		}
	}
};

listElement* list::is_uid_known(byte* uid, uint8_t size){
	if(first==NULL){
		return NULL; // if there is no element, return NULL
	}
	listElement* e=first; // start with the first
	while(!e->compare_uid(uid, size)){ // if uid is not the same
		if(e->get_next()!=NULL){ // check if there is another element
			e = e->get_next(); // check the next element
		} else {
			return NULL; // if there is no other element, return NULL
		}
	}
	return e; // if we reach this we have a match
}

bool list::remove_uid(listElement* element){
	if(first==NULL){
		return false;
	}
	listElement* last = first;
	listElement* e;
	while(last!=NULL){
		if(last->get_next() == element){
			e = last->get_next()->get_next();
			delete last->get_next();
			last->set_next(e);
			return true;
		} else {
			last = last->get_next();
		}
	}
	return false;
}
