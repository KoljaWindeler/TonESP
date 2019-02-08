#ifndef WIFI_h
#define WIFI_h

#include <Arduino.h>
#include "../lib/PubSubClient/src/PubSubClient.h"
#include <ESP8266WiFi.h>


bool wifi_connected();
bool wifi_scan_connect();
void wifi_setup();

#endif
