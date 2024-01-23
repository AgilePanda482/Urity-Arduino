#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include <WebSocketsClient.h>
#include <SocketIOclient.h>

#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG(...) DEBUG_ESP_PORT( __VA_ARGS__)
#else 
#define DEBUG_MSG(...)
#endif
#define DEBUG_ESP_PORT Serial


#define RST_PIN 27
#define SS_PIN 5

String lecturaUID = "";
bool rfidStatus = false;
unsigned long messageTimestamp = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
SocketIOclient socketIO;


#define USE_SERIAL Serial

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
String almacenarUID(String variableUID);
void sendMessage();


void setup() {
	USE_SERIAL.begin(115200);
  USE_SERIAL.print("hola//////////////////////");

	//Serial.setDebugOutput(true);
	USE_SERIAL.setDebugOutput(true);

	USE_SERIAL.println();
	USE_SERIAL.println();
	USE_SERIAL.println();

	for(uint8_t t = 4; t > 0; t--) {
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}

  WiFiMulti.addAP("", "");


	//WiFi.disconnect();
	while(WiFiMulti.run() != WL_CONNECTED) {
        
    Serial.println("reconectando...");
	}

	socketIO.setExtraHeaders("Authorization: 1234567890");
	socketIO.begin("192.168.100.22", 3000, "/socket.io/?EIO=4");


	// event handler
	//socketIO.onEvent(webSocketEvent);
  	socketIO.onEvent(socketIOEvent);

	// use HTTP Basic Authorization this is optional remove if not needed
	//webSocket.setAuthorization("user", "Password");

	// try ever 5000 again if connection has failed
	//webSocket.setReconnectInterval(3000);


  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Setup iniciado correctamente");

}


void loop(){
	//	webSocket.loop();
	socketIO.loop();

  if ( !mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()){
    return;
  }

  

  uint64_t now = millis();
  if(now - messageTimestamp > 5000) {
    lecturaUID = almacenarUID(lecturaUID);
  }

  sendMessage();

}

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length){
  switch(type){
    case sIOtype_DISCONNECT:
      Serial.printf("[IOc] Disconnected!\n");
    break;
    case sIOtype_CONNECT:
      Serial.printf("[IOc] Connected to url: %s\n", payload);
      // join default namespace (no auto join in Socket.IO V3)
      socketIO.send(sIOtype_CONNECT, "/");
      // socketIO.send("hiiii");
    break;
    case sIOtype_EVENT:
      Serial.printf("[IOc] get event: %s\n", payload);
    break;
    case sIOtype_ACK:
      Serial.printf("[IOc] get ack: %u\n", length);
      hexdump(payload, length);
    break;
    case sIOtype_ERROR:
      Serial.printf("[IOc] get error: %u\n", length);
      hexdump(payload, length);
    break;
    case sIOtype_BINARY_EVENT:
      Serial.printf("[IOc] get binary: %u\n", length);
      hexdump(payload, length);
    break;
    case sIOtype_BINARY_ACK:
      Serial.printf("[IOc] get binary ack: %u\n", length);
      hexdump(payload, length);
    break;
  }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols) {
	const uint8_t* src = (const uint8_t*) mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			USE_SERIAL.printf("\n[0x%0hola8X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

String almacenarUID(String variableUID){
  // muestra texto UID en el monitor:
  Serial.print("\n");
  Serial.print("UID:");

  // bucle recorre de a un byte por vez el UID (Esto lo imprime al monitor)
  for(byte i = 0; i < mfrc522.uid.size; i++){
    //si el byte leido es menor a 0x10
    if(mfrc522.uid.uidByte[i] < 0x10){
      //imprime espacio en blanco y numero cero
      Serial.print(" 0");
      variableUID += "0";

    }else{
      Serial.print(" ");
    }

    //imprime el byte del UID leido en hexadecimal
    Serial.print(mfrc522.uid.uidByte[i], HEX);

    // almacena en String el byte del UID leido
    variableUID += String(mfrc522.uid.uidByte[i], HEX);

  }
  Serial.print("\t");
  variableUID.toUpperCase();

  return variableUID;
}


void sendMessage()
{
  Serial.println("\n");
  Serial.println("Se enviara un JSON a Javascript con la siguiente información: ");
  Serial.println(lecturaUID);
  // Declare a buffer to hold the result
  String output;
  DynamicJsonDocument doc(1024);

  doc["rfid_tag_id"] = lecturaUID;
  serializeJson(doc, output);

  // Send event        
  socketIO.sendEVENT(output);

  // Print JSON for debugging
  USE_SERIAL.println(output);
}