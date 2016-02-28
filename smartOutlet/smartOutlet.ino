#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <WebSocketsServer.h>
#include <Hash.h>

#include <ArduinoOTA.h>
#include <ArduinoOTA.h>
#include "Thread.h"
#include "ThreadController.h"
#include "Base64.h"

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
	val.concat(":");
	val.concat(current+20);
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

float readCurrent(int pin){
	// they are PNP transistors, to turn them on (connect current sensor to ADC) you write a low
	if(pin == 1){
		digitalWrite(current1, LOW);
		digitalWrite(current2, HIGH);
		return maxCurrent();
	}
	else if(pin==2){
		digitalWrite(current2, LOW);
		digitalWrite(current1, HIGH);
		return maxCurrent();
	}
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
	digitalWrite(chipSelectPinADC, HIGH);	// ADC off
	digitalWrite(current1, HIGH);
	digitalWrite(current2, HIGH);	// current sensors disconnected


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
	// run readCurrent(1); - specify current 1 or two. Returns float. 
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

char input2[] =
"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\n"
"";

void handleGZIP() {
  Serial.println("GZIP page served");
  Serial.println(input2);
  int input2Len = sizeof(input2);
  Serial.print("input2len: ");
  Serial.println(input2Len);
  int decodedLen = base64_dec_len(input2, input2Len);
  Serial.println(decodedLen);
  char decoded[decodedLen];
  base64_decode(decoded, input2, input2Len);
  Serial.println(decoded);

  webServer.sendHeader("Content-Encoding", "gzip", false);
  webServer.send_c(200, "text/html", decoded, decodedLen);
}
