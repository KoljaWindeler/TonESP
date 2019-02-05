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
}

byte* listElement::get_uuid(){
	return m_uidByte;
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

uint8_t listElement::get_mode(){
	return m_mode;
}

uint8_t listElement::get_folder(){
	return m_folder;
}

uint16_t listElement::get_track(){
	return m_track;
}

void listElement::set_mode(uint8_t mode){
	m_mode = mode;
}

void listElement::set_folder(uint8_t folder){
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
};

list::~list(){};

bool list::store(){
	Serial.println("storing /cards.txt");
	File f = SPIFFS.open("/cards.txt", "w");
	if (!f) {
    Serial.println("file open failed");
	} else {
		listElement* e = first;
		while(e!=NULL){
			Serial.println("Storing 14 bytes");
			f.write(e->get_uuid(), 10);
			f.write(e->get_mode());
			f.write(e->get_folder());
			uint16_t t=e->get_track();
			f.write((uint8_t*)&t,2);
			Serial.printf("Mode: %i, Folder: %02i, Track: %03i\r\n",e->get_mode(), e->get_folder(), e->get_track());
			e = e->get_next();
		}
		f.close();
	}
	return true;
};

bool list::load(){
	Serial.println("loading /cards.txt");
	if(SPIFFS.exists("/cards.txt")){
		Serial.println("File existed");
		File f = SPIFFS.open("/cards.txt", "r");
		byte uid[14];
		while(f.available()>=14){
			Serial.println("Reading 14 bytes");
			f.read(uid, 14);
			listElement* e = new listElement(uid,10);
			e->set_mode(uid[10]);
			e->set_folder(uid[11]);
			e->set_track((((uint16_t)uid[12])<<4) & uid[13]);
			Serial.printf("Mode: %i, Folder: %02i, Track: %03i\r\n",e->get_mode(), e->get_folder(), e->get_track());
			add_uid(e);
		}
		f.close();
	}
	return true;
};

void list::add_uid(listElement* element){
	listElement* e=first;
	while(e != NULL){
		if(e->get_next() == NULL){
			e->set_next(element);
			length++;
		} else {
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
