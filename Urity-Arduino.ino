//Arduino Libraries
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

//Wifi Libraries
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

//Web Libraries
#include <ArduinoJson.h>
#include <SocketIOclient.h>

//Password Libraries
#include "page.h"

/*#ifdef DEBUG_ESP_PORT
#define DEBUG_MSG(...) DEBUG_ESP_PORT( __VA_ARGS__)
#else 
#define DEBUG_MSG(...)
#endif
#define DEBUG_ESP_PORT Serial*/

//RC522 pins 
#define RST_PIN 27
#define SS_PIN 5

//Variables
String lecturaUID = "";
bool rfidStatus = false;
const uint32_t TiempoEsperaWifi = 5000;
unsigned long before = 0;
uint64_t now;

//Objects
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiMulti wifiMulti;
WebSocketsClient webSocket;
SocketIOclient socketIO;

//Functions
void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
String almacenarUID(String variableUID);
void sendMessage();

//Serial mode
#define USE_SERIAL Serial

void setup() {
	USE_SERIAL.begin(115200);

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

  //Internet Conexions
  wifiMulti.addAP(ssid_1, password_1);
  wifiMulti.addAP(ssid_2, password_2);
  wifiMulti.addAP(ssid_3, password_3);

  WiFi.mode(WIFI_STA);

  Serial.print("Conectando a Wifi...");
  
  while (wifiMulti.run(TiempoEsperaWifi) != WL_CONNECTED) {
    Serial.print(".");
  }
  Serial.println("...Conectado");
  Serial.print("SSID:");
  Serial.print(WiFi.SSID());
  Serial.print(" ID:");
  Serial.println(WiFi.localIP());

  //socket ip conexion
	socketIO.setExtraHeaders("Authorization: 1234567890");
	socketIO.begin("192.168.62.15", 3000, "/socket.io/?EIO=4");


	// event handler
  socketIO.onEvent(socketIOEvent);

	// use HTTP Basic Authorization this is optional remove if not needed
	//webSocket.setAuthorization("user", "Password");

	// try ever 5000 again if connection has failed
	//webSocket.setReconnectInterval(5000);

  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Setup iniciado correctamente");
}


void loop() {
  socketIO.loop();      
  uint64_t now = millis();

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()){
    if(now - before > 3000) {
      before = now;
      lecturaUID = almacenarUID(lecturaUID);

      sendMessage();
    }
  }
  lecturaUID = "";
}

//WebsocketIO function
void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  switch(type){
    case sIOtype_DISCONNECT:
      Serial.printf("[IOc] Disconnected!\n");
    break;
    case sIOtype_CONNECT:
      Serial.printf("[IOc] Connected to url: %s\n", payload);
      // join default namespace (no auto join in Socket.IO V3)
      socketIO.send(sIOtype_CONNECT, "/");       
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

//Read NFC card Function
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
  Serial.println("Se enviara un JSON a Node.js con la siguiente información: ");
  Serial.println(lecturaUID);

  // creat JSON message for Socket.IO (event)
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
          
  // add event name
  // Hint: socket.on('event_name', ....
  array.add("event_name");

  // add payload (parameters) for the event
  JsonObject param1 = array.createNestedObject();
  param1["UID"] = lecturaUID;

  // JSON to String (serializion)
  String output;
  serializeJson(doc, output);

  // Send event        
  socketIO.sendEVENT(output);

  // Print JSON for debugging
  USE_SERIAL.println(output);
}