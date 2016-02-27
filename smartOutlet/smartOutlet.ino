#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <WebSocketsServer.h>
#include <Hash.h>

#include <ArduinoOTA.h>

ESP8266WebServer webServer(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#define USE_WIFI_STA//USE_WIFI_AP

#if defined(USE_WIFI_AP)
IPAddress apIP(192, 168, 1, 1);
IPAddress netMask(255, 255, 255, 0);
#elif defined(USE_WIFI_STA)
const char* ssid = "nwHACKS";
const char* pass = "welcometoUBC!";
#endif

const int bluePin = 13;
const int redPin = 15;
const int greenPin = 12;
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED: {
			break;
		} case WStype_CONNECTED: {
			IPAddress ip = webSocket.remoteIP(num);
			break;
		} case WStype_TEXT: {

			String text = String((char *) &payload[0]);
			if(text=="LED"){
				digitalWrite(13,HIGH);
				delay(500);
				digitalWrite(13,LOW);
				Serial.println("led just lit");
				webSocket.sendTXT(num, "led just lit", length);
			}

			if(text.startsWith("x")){
				String xVal=(text.substring(text.indexOf("x")+1,text.length())); 
				int xInt = xVal.toInt();
				analogWrite(redPin,xInt); 
				Serial.println(xVal);
				webSocket.sendTXT(num, "red changed", length);
			}

			if(text.startsWith("y")){
				String yVal=(text.substring(text.indexOf("y")+1,text.length())); 
				int yInt = yVal.toInt();
				analogWrite(greenPin,yInt); 
				Serial.println(yVal);
				webSocket.sendTXT(num, "green changed", length);
			}

			if(text.startsWith("z")){
				String zVal=(text.substring(text.indexOf("z")+1,text.length())); 
				int zInt = zVal.toInt();
				analogWrite(bluePin,zInt); 
				Serial.println(zVal);
				webSocket.sendTXT(num, "blue changed", length);
			}

			webSocket.sendTXT(num, payload, length);
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


void setup() {
	Serial.begin(115200);
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
	webServer.begin();
	Serial.println("WebSocket server started\nHTTP server started\nReady...");
}

void loop() {
	webSocket.loop();
	webServer.handleClient();
	ArduinoOTA.handle();
}

void handleRoot() {
  Serial.println("Page served");
  String toSend = "Hello!";
  webServer.send(200, "text/html", toSend);
}
