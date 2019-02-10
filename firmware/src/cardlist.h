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
		uint16_t get_mode();
		uint16_t get_folder();
		uint16_t get_track();
		uint16_t* get_mode_p();
		uint16_t* get_folder_p();
		uint16_t* get_track_p();
		void set_mode(uint16_t mode);
		void set_folder(uint16_t folder);
		void set_track(uint16_t track);
		void set_uuid(byte* uid, uint8_t size);
		void set_next(listElement* element);
	private:
		byte		m_uidByte[10];
		uint8_t	m_uidByteLength;
		listElement* m_next;
		uint16_t m_folder;
		uint16_t m_track;
		uint16_t m_mode;
};


class list {
public:
	list();
	~list();
	bool store();
	bool load();
	void add_uid(listElement* element);
	bool remove_uid(listElement* element);
	listElement* is_uid_known(byte* uid, uint8_t size);
private:
	uint16_t length;
	listElement* first;
};
#endif
