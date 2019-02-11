#ifndef CARDLIST_h
#define CARDLIST_h

#include <Arduino.h>
#include <FS.h>   // Include the SPIFFS library



class listElement {
	public:
		listElement(byte* uid, uint8_t size);
		~listElement();
		listElement* get_next();
		bool compare_uid(byte* uid, uint8_t size);
		byte* get_uuid();
		uint8_t get_uuidLength();
		uint8_t get_mode();
		uint8_t get_folder();
		uint8_t get_track();
		uint8_t* get_mode_p();
		uint8_t* get_folder_p();
		uint8_t* get_track_p();
		void set_mode(uint8_t mode);
		void set_folder(uint8_t folder);
		void set_track(uint8_t track);
		void set_uuid(byte* uid, uint8_t size);
		void set_next(listElement* element);
		void dump_ascii(uint8_t* a);
		uint8_t is_temporary;
	private:
		byte		m_uidByte[10];
		uint8_t	m_uidByteLength;
		listElement* m_next;
		uint8_t m_folder;
		uint8_t m_track;
		uint8_t m_mode;
};


class list {
public:
	list();
	~list();
	bool store();
	bool load();
	void add_uid(listElement* element);
	bool remove_uid(listElement* element);
	void clear();
	listElement* get_element_nr(uint8_t nr);
	listElement* is_uid_known(byte* uid, uint8_t size);
private:
	uint16_t length;
	listElement* first;
};
#endif
