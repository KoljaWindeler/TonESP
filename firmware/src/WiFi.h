#ifndef WIFI_h
#define WIFI_h

#include <Arduino.h>
#include "../lib/PubSubClient/src/PubSubClient.h"
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>


#define UNIT_TO_PC 'r'
#define PC_TO_UNIT 's'

#define ACTION_CREATE 		1
#define ACTION_DELETE 		2
#define ACTION_FORMAT 		3
#define ACTION_EMULATE 		4

#define WIFI_STATE_DISCONNECTED 0
#define WIFI_STATE_CONNECTING 	1
#define WIFI_STATE_CONNECTED 		2

#define MSG_BUFFER_SIZE          60   // mqtt messages max char size
#define TOPIC_BUFFER_SIZE        64   // mqtt topic buffer
#define PUBLISH_TIME_OFFSET      200  // ms timeout between two publishes

bool wifi_connected();
bool wifi_scan_connect();
void wifi_setup();
char * build_topic(const char * topic, uint8_t pc_shall_R_or_S);
char * build_topic(const char * topic, uint8_t pc_shall_R_or_S, bool with_dev);
extern void play(uint8_t typ);
void callback(char * p_topic, byte * p_payload, uint16_t p_length);



#endif
