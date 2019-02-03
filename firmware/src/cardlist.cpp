#include <cardlist.h>


listElement::listElement(byte* uid){
	// copy uid
	set_uuid(uid);

	// set information
	m_next=NULL;
	m_folder=0;
	m_track=0;
	m_mode=0;
};

listElement::~listElement(){};

void listElement::set_uuid(byte* uid){
	for(uint8_t i=0;i<10;i++){
		m_uidByte[i]=*uid;
		uid++;
	}
}

listElement* listElement::get_next(){
	return m_next;
}

bool listElement::compare_uid(byte* uid){
	for(uint8_t i=0;i<10;i++){
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
};

list::~list(){};

bool list::store(){
	return true;
};

bool list::load(){
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

listElement* list::is_uid_known(byte* uid){
	if(first==NULL){
		return NULL; // if there is no element, return NULL
	}
	listElement* e=first; // start with the first
	while(!e->compare_uid(uid)){ // if uid is not the same
		if(e->get_next()!=NULL){ // check if there is another element
			e = e->get_next(); // check the next element
		} else {
			return NULL; // if there is no other element, return NULL
		}
	}
	return e; // if we reach this we have a match
}
