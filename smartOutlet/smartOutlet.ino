#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <WebSocketsServer.h>
#include <Hash.h>

#include <ArduinoOTA.h>
#include <ArduinoOTA.h>
#include "Thread.h"
#include "ThreadController.h"

#include <SPI.h>


ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define SENSOR_POLL_RATE_HZ 1
#define USE_WIFI_STA//USE_WIFI_AP

// Some variables for the current measurement
const int chipSelectPinADC = 15;
unsigned int result = 0;
byte inByte = 0;

// Setting up the pins
const int relay1 = 5;
const int relay2 = 4;
const int current1 = 13;
const int current2 = 2;

#if defined(USE_WIFI_AP)
IPAddress apIP(192, 168, 1, 1);
IPAddress netMask(255, 255, 255, 0);
#elif defined(USE_WIFI_STA)
const char* ssid = "nwHACKS";
const char* pass = "welcometoUBC!";
#endif

ThreadController threadController = ThreadController();

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED: {
			break;
		} case WStype_CONNECTED: {
			IPAddress ip = webSocket.remoteIP(num);
			break;
		} case WStype_TEXT: {

			String text = String((char *) &payload[0]);

			// r[relay][state]
			if(text.startsWith("r") && text.length() == 3) {
				char relay = text.charAt(1);
				char state = text.charAt(2);
				String res = "setting relay ";
				res.concat(relay);
				res.concat(" to state ");
				res.concat(state);
				Serial.println(res);
				webSocket.sendTXT(num, res);
			}

			if(text=="LED"){
				Serial.println("led just lit");
				webSocket.sendTXT(num, "led just lit");
			}

			if(text.startsWith("x")){
				String xVal=(text.substring(text.indexOf("x")+1,text.length())); 
				int xInt = xVal.toInt();
				Serial.println(xVal);
				webSocket.sendTXT(num, "red changed");
			}

			if(text.startsWith("y")){
				String yVal=(text.substring(text.indexOf("y")+1,text.length())); 
				int yInt = yVal.toInt();
				Serial.println(yVal);
				webSocket.sendTXT(num, "green changed");
			}

			if(text.startsWith("z")){
				String zVal=(text.substring(text.indexOf("z")+1,text.length())); 
				int zInt = zVal.toInt();
				Serial.println(zVal);
				webSocket.sendTXT(num, "blue changed");
			}

			webSocket.broadcastTXT(payload, length);
			break;
		} case WStype_BIN: {

			hexdump(payload, length);

			// echo data back to browser
			webSocket.sendBIN(num, payload, length);
			break;
		}
	}

}


void currentThreadCallback() {
	const int current = analogRead(A0);
	String val = "data:current:";
	val.concat(current);
	Serial.println(val);
	webSocket.broadcastTXT(val);
}

unsigned int readADC(){
  	digitalWrite(chipSelectPinADC, LOW);	// Tell ADC to take a reading
	result = SPI.transfer(0x00);	// Get first 8 bits
	result = result << 8;	// Shift first 8 bits over
	inByte = SPI.transfer(0x00);	// Get second 8 bits
	result = result | inByte;	// Slam them together
	digitalWrite(chipSelectPinADC, HIGH);	// ADC off
	result = result >> 1;	// Get rid of bad bit at the end
	result = result & 0b0000111111111111;	// Snag the 12 bits of info
	return result;
	// Serial.println(result); // can bring this in if debugging is needed
	// delay(100);  // avoid the use of delays, use yield(); instead
	result = 0x00;  
}

float maxCurrent(){
  int maxI = 0;
  int minI = 4000;
  int currentIn = 0;
  for(int i = 0; i < 50; i++){
    currentIn = readADC();
    if(currentIn > maxI) maxI = currentIn;
    else if(currentIn < minI) minI = currentIn;
    delay(5);	// may need to remove this delay
  }

  int dcOffset = minI + ((maxI - minI)/2);  // find the DC offset (the value the sensor gives at zero current)
  int difference = maxI - dcOffset; //find the amplitude of the + current

  const float vPerDiv = 0.00081764;  // convert ADC to voltage
  const float ampsPerVolt = 0.09;

  float current = difference * vPerDiv; // turn it into a voltage
  current = current / ampsPerVolt;    // scale the voltage into the ADC to find actual current
  current *= 1000;  // turn into Milliamps
  
  if(current < 150) current = 0;  // get rid of any small currents (caused by ADC noise)
  
  current /= 1.4142;  // this gets us into the RMS current

  return current;
}

void setup() {
	Serial.begin(115200);

	// SPI and Pin Setup
	SPI.begin();
	SPI.setFrequency(1000000);
	SPI.setBitOrder(MSBFIRST);
	pinMode(chipSelectPinADC, OUTPUT);
	pinMode(relay1, OUTPUT);
	pinMode(relay2, OUTPUT);
	pinMode(current1, OUTPUT);
	pinMode(current2, OUTPUT);
	digitalWrite(chipSelectPinADC, HIGH);


	#ifdef defined(USE_WIFI_AP)
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(apIP, apIP, netMask); 
	WiFi.softAP("smartOutlet - Booting");
	#elif defined(USE_WIFI_STA)
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connecting...");
		delay(1000);
	}
	#endif
	
	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname("smartoutlet");
	ArduinoOTA.onStart([]() {
		Serial.println("OTA Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nOTA End");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("OTA Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.begin();


	Serial.print("\nIP address: ");
	#if defined(USE_WIFI_AP)
	Serial.println(WiFi.softAPIP());
	#elif defined(USE_WIFI_STA)
	Serial.println(WiFi.localIP());
	#endif

	webSocket.begin();
	webSocket.onEvent(webSocketEvent);


	webServer.on("/", handleRoot);
	webServer.on("/gzip", handleGZIP);
	webServer.begin();
	Serial.println("WebSocket server started\nHTTP server started\nReady...");

	// Threading
	Thread *currentThread = new Thread();
	currentThread->setInterval(1000/SENSOR_POLL_RATE_HZ);
	currentThread->onRun(currentThreadCallback);

	threadController.add(currentThread);
}

void loop() {

	// Current get:
	// Turn current1 off, current2 on (or vice-versa)
	// run maxCurrent - returns a float 
	// use digitalwrite(relay1, HIGH), etc to turn on relays

	webSocket.loop();
	webServer.handleClient();
	ArduinoOTA.handle();
	threadController.run();
}

void handleRoot() {
  Serial.println("Page served");
  String toSend = "Hello!";
  webServer.send(200, "text/html", toSend);
}

void handleGZIP() {
  Serial.println("GZIP page served");
  String toSend = "fake gzip";
  webServer.send(200, "text/html", toSend);
}
