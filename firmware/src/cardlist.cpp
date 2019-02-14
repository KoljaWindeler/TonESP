#include <cardlist.h>
#include "globals.h"

listElement::listElement(byte* uid, uint8_t size){
	// copy uid
	set_uuid(uid,size);

	// set information
	m_next=NULL;
	m_folder=0;
	m_track=0;
	m_mode=0;
	is_temporary=1;
	memset(m_userdata,0x00,10);
};

listElement::~listElement(){};

void listElement::set_uuid(byte* uid, uint8_t size){
	if(size>10){
		size = 10;
	}
	memcpy(m_uidByte,uid,size);
	m_uidByteLength=size;
	//Serial.printf("copied %i uuid bytes",size);
}

void listElement::set_userdata(byte* data){
	memcpy(m_userdata,data,10); // 10 is fixed length
	//Serial.printf("copied %i uuid bytes",size);
}

byte* listElement::get_uuid(){
	return m_uidByte;
}

byte* listElement::get_userdata(){
	return m_userdata;
}

void listElement::dump_ascii(uint8_t* a){
	for(uint8_t i=0;i<10;i++){
		sprintf((char*)a,"%02X",m_uidByte[i]);
		a+=2;
	}
	a+=sprintf((char*)a,",%02i,%02i,%03i,",m_mode%100, m_folder%100, m_track%1000);
	for(uint8_t i=0;i<10;i++){
		sprintf((char*)a,"%02X",m_userdata[i]);
		a+=2;
	}
	// just to make sure that we limit amount of chars
	// 20+1+2+1+2+1+3+1+20 = 51
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
	/*
	Serial.print("compare uids this card: ");
	for(uint8_t i=0;i<10;i++){
		Serial.printf("0x%x ",m_uidByte[i]);
	}
	Serial.printf(" this mem %i\r\n",this);
	*/


	for(uint8_t i=0;i<size;i++){
		//Serial.printf("%02x,%02x ",m_uidByte[i],*uid);
		if(m_uidByte[i]!=*uid){
			//Serial.println("false");
			return false;
		}
		uid++;
	}
	//Serial.println("true");
	return true;
}

uint8_t listElement::get_mode(){
	return m_mode;
}

uint8_t* listElement::get_mode_p(){
	return &m_mode;
}

uint8_t listElement::get_folder(){
	return m_folder;
}

uint8_t* listElement::get_folder_p(){
	return &m_folder;
}

uint8_t listElement::get_track(){
	return m_track;
}

uint8_t* listElement::get_track_p(){
	return &m_track;
}

void listElement::set_mode(uint8_t mode){
	m_mode = mode;
}

void listElement::set_folder(uint8_t folder){
	m_folder = folder;
}

void listElement::set_track(uint8_t track){
	m_track = track;
}

void listElement::set_next(listElement* element){
	if(this == element){
		debug_println("card",0,"Critical error, Loop detected!!");
		return;
	}
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

// store the list into the SPIFFS
bool list::store(){
	debug_print(("card"),0,F("storing /cards.txt .. "));
	//delay(200);
	uint16_t i=0;
	File f = SPIFFS.open("/cards2.txt", "w");
	if (!f) {
    debug_println(("card"),0,F("file open failed"));
	} else {
		listElement* e = first;
		uint8_t temp[23];
		uint8_t* a;
		while(e!=NULL){
			a = e->get_uuid();
			for(uint8_t ii=0;ii<10;ii++){
				temp[ii]=*a;
				a++;
			}
			temp[10]=e->get_mode();
			temp[11]=e->get_folder();
			temp[12]=e->get_track();
			a = e->get_userdata(); // 10 byte fixed
			for(uint8_t ii=13;ii<23;ii++){
				temp[ii]=*a;
				a++;
			}

			//Serial.println("Storing 23 bytes");
			//delay(200);
			f.write(temp, 23);
			//for(uint i=0; i<23; i++){
			//	Serial.printf("%02x ",temp[i]);
			//}
			//Serial.printf("Mode: %i, Folder: %02i, Track: %03i\r\n",e->get_mode(), e->get_folder(), e->get_track());
			//delay(500);
			if(e!=e->get_next()){
				e = e->get_next();
			} else {
				Serial.println("LOOP detected");
				break;
			}
			i++;
		}
		f.close();
		SPIFFS.remove("/cards.txt");
		SPIFFS.rename("/cards2.txt","/cards.txt");
		Serial.printf("completed, %i cards stored\r\n",i);
		delay(200);
	}
	return true;
};

void list::clear(){
	// clear list
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
			//Serial.printf("Deleting card nr %i from RAM\r\n",i);
			delete e2;
			ii++;
		}
		if(i==0){
			first = NULL;
		} else {
			e->set_next(NULL);
		}
	}
	debug_printf("card", COLOR_GREEN, "%i cards deleted from RAM\r\n",ii);
}

// load cards from SPIFFS
bool list::load(){
	uint16_t cards_loaded=0;
	clear();

	debug_println(("card"),COLOR_GREEN,F("loading /cards.txt"));
	if(SPIFFS.exists("/cards.txt")){
		//Serial.println("File existed");
		File f = SPIFFS.open("/cards.txt", "r");
		byte temp[23];
		while(f.available()>=23){
			cards_loaded++;
			//Serial.println("Reading 23 bytes");
			//delay(200);
			f.read(temp, 23);
			listElement* e = new listElement(temp,10);
			//Serial.println("set mode");
			//delay(200);
			e->set_mode(temp[10]);
			e->set_folder(temp[11]);
			e->set_track(temp[12]);
			e->set_userdata(&temp[13]);

			debug_printf("card",COLOR_YELLOW,"(%03i) Mode: %i, Folder: %02i, Track: %03i, UUID: ", cards_loaded, e->get_mode(), e->get_folder(), e->get_track());
			for(uint i=0; i<10; i++){
				Serial.printf("%02X",temp[i]);
			}
			Serial.print(", Userdata: ");
			for(uint i=13; i<23; i++){
				Serial.printf("%02X",temp[i]);
			}
			Serial.println("");
			add_uid(e);
		}
		f.close();
		debug_printf("card",COLOR_GREEN,"%i cards loaded to RAM\r\n",cards_loaded);
		delay(200);
	}
	return true;
};


// add a new element to the list
void list::add_uid(listElement* element){
	uint8_t* m_uidByte = element->get_uuid();
	//Serial.printf("this card: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ",
	//m_uidByte[0], m_uidByte[1], m_uidByte[2], m_uidByte[3], m_uidByte[4], m_uidByte[5], m_uidByte[6], m_uidByte[7], m_uidByte[8], m_uidByte[9]);
	element->is_temporary = false;

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
	//DMSG_F("Trying to delete card %i\r\n",element);
	if(first==NULL){
		//DMSG_LN("card list empty, exiting");
		return false;
	}
	listElement* e;
	if(first==element){
		//DMSG_LN("matches first, deleting");
		e=first->get_next();
		delete first;
		first = e;
		return true;
	}
	listElement* last = first;
	while(last!=NULL){
		//DMSG_F("loading card adr %i ",last);
		//DMSG_F("next card is %i ",last->get_next());
		if(last->get_next() == element){
			e = last->get_next()->get_next();
			delete last->get_next();
			last->set_next(e);
			//DMSG_F("\r\ngetting next next %i and store it as next of %i\r\n",e,last);
			return true;
		} else {
			last = last->get_next();
		}
		Serial.println();
	}
	return false;
}

// not sure when i will ever use it, but this should give me a quick way to load a card by nr
// IN: uint_t nr = number of card in data chain, 0 based ..
// OUT: pointer to card, or NULL if not found
listElement* list::get_element_nr(uint8_t nr){
	uint8_t i=0;
	listElement* e=first;
	while(i<=nr && e!=NULL){
		if(i==nr){
			return e;
		} else {
			i++;
			e = e->get_next();
		}
	}
	return NULL;
}
