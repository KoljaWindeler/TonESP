#include "WiFi.h"

const int maxSize=20;
uint8_t wifi_status=0;
char wifi_config[6][maxSize];
PubSubClient client;


void wifi_setup(){
	//WiFi.disconnect();
	//WiFi.mode(WIFI_STA);
	sprintf(wifi_config[0],"IOT255");
	sprintf(wifi_config[1],"windycarrot475");
	sprintf(wifi_config[2],"IOT0");
	sprintf(wifi_config[3],"windycarrot475");
	sprintf(wifi_config[4],"IOT1");
	sprintf(wifi_config[5],"windycarrot475");
	//delay(100);
}

bool wifi_scan_connect(){
	if(!wifi_connected()){
		WiFi.mode(WIFI_STA);
		int n = WiFi.scanNetworks();
		if(n>0){
			uint8_t best=0;
			int8_t best_value=WiFi.RSSI(0);
			for (int i = 0; i < n; i++)
			{
				//Serial.println(WiFi.SSID(i));
				//Serial.println(WiFi.RSSI(i));

				for(uint8_t ii=0; ii<3; ii++){
					if(WiFi.SSID(i).compareTo(wifi_config[ii*2])){
						if(WiFi.RSSI(i)>best_value){
							//Serial.print("Best WiFi updated to ");
							//Serial.println(i);
							best=i;
							best_value=WiFi.RSSI(i);
						}
					}
				}
			}
			Serial.printf("Connect to %s -> %s\r\n",wifi_config[best*2],wifi_config[best*2+1]);
			WiFi.begin(wifi_config[best],wifi_config[best]);
			/*while (WiFi.status() != WL_CONNECTED)
		  {
		    delay(500);
		    Serial.print(".");
		  }*/
			Serial.println(WiFi.localIP());
			Serial.println(WiFi.status());
		}
	}
	//WiFi.begin("IOT0", "windycarrot475");
}

void callback(char * p_topic, byte * p_payload, uint16_t p_length){
	Serial.println("asd");
	delay(100);
	return;
}

bool wifi_connected(){
	if(WiFi.status() == WL_CONNECTED){
		if(wifi_status==0 || !client.connected()){
			wifi_status = 1;
			Serial.println("Wifi is now connected");
			delay(200);
			client.setServer(IPAddress(192,168,2,84), 1883);
			Serial.println("Wifi1");
			delay(200);
			client.setCallback(callback); // in main.cpp
			Serial.println("Wifi2");
			delay(200);
			Serial.println(WiFi.localIP());
			if(client.connect("TonESP1", "ha", "ah")){
			//if(client.connect("TonESP1", "ha", "ah", "TonESP1/INFO", 0, true, "lost signal")){
				Serial.println("Wifi3");
				delay(200);
				Serial.println("MQTT connected");
			}
			Serial.println("Wifi4");
			delay(200);
		}
		return true;
	} else {
		wifi_status = 0;
		return false;
	}
}
