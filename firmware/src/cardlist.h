#ifndef CARDLIST_h
#define CARDLIST_h

#include <Arduino.h>

class listElement {
	public:
		listElement(byte* uid);
		~listElement();
		listElement* get_next();
		bool compare_uid(byte* uid);
		uint8_t get_mode();
		uint8_t get_folder();
		uint16_t get_track();
		void set_mode(uint8_t mode);
		void set_folder(uint8_t folder);
		void set_track(uint16_t track);
		void set_uuid(byte* uid);
		void set_next(listElement* element);
	private:
		byte		m_uidByte[10];
		listElement* m_next;
		uint8_t m_folder;
		uint16_t m_track;
		uint8_t m_mode;
};


class list {
public:
	list();
	~list();
	bool store();
	bool load();
	void add_uid(listElement* element);
	listElement* is_uid_known(byte* uid);
private:
	uint16_t length;
	listElement* first;
};
#endif
