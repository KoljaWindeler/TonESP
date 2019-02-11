#ifndef SETTINGS_h
#define SETTINGS_h

#include <Arduino.h>
#include <FS.h>   // Include the SPIFFS library


class settings {
	public:
		settings();
		~settings();
		bool load();
		bool store();
		void set_max_volume(uint8_t v);
		void print_settings();
		uint8_t get_max_volume();
		uint8_t m_current_volume;
		uint8_t m_locked;
	private:
		uint8_t m_max_volume;
	};

	#endif
