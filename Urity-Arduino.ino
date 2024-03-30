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

//Piezo Pin
#define piezo 33

//Chapa Pin
#define chapa 13

//Variables
String lecturaUID = "";
bool rfidStatus = false;
uint64_t now;
unsigned long beforeRead = 0;
unsigned long beforeDoor = 0;
bool chapaStatus;

//Constantes
const uint32_t TiempoEsperaWifi = 5000;
const int chapaTime = 4000;
const int intervalRFID = 3000;

//Objects
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiMulti wifiMulti;
WebSocketsClient webSocket;
SocketIOclient socketIO;

//Functions
void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
void readRFID();
String almacenarUID(String variableUID);
void sendMessage();
void receiveMessege(uint8_t* payload, size_t length);
void chapaControl(bool status);

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
	socketIO.begin("192.168.100.26", 3000, "/socket.io/?EIO=4");


	// event handler
  socketIO.onEvent(socketIOEvent);

	// use HTTP Basic Authorization this is optional remove if not needed
	//webSocket.setAuthorization("user", "Password");

	// try ever 5000 again if connection has failed
	//webSocket.setReconnectInterval(5000);

  pinMode(chapa, OUTPUT);
  digitalWrite(chapa, LOW);
  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Setup iniciado correctamente");
}

void loop() {
  socketIO.loop();      
  now = millis();

  readRFID();
  
  if (chapaStatus)
  {
    digitalWrite(chapa, HIGH);
  }
  
  if(now - beforeDoor > chapaTime) {
    beforeDoor = now;
    
    digitalWrite(chapa, LOW);
    chapaStatus = 0;
    mfrc522.PCD_Init();
  }
}


/*------------FUNCTIONS---------------*/

//WebsocketIO function
void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
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
      receiveMessege(payload, length);
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

void readRFID() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()){
    if (millis() - beforeRead > intervalRFID){
      beforeRead = millis();
      lecturaUID = almacenarUID();
      sendMessage();
      lecturaUID = "";
      mfrc522.PICC_HaltA();
    }
  }
}


//Read NFC card Function
String almacenarUID() {
  String variableUID;
  Serial.println("\nUID:"); // Utiliza println para imprimir una nueva línea automáticamente

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    // Utiliza operador ternario para agregar '0' si es necesario
    String hexByte = String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
    hexByte.toUpperCase(); // Convierte a mayúsculas antes de agregar a variableUID

    Serial.print(" "); // Imprime un espacio en blanco
    Serial.print(hexByte); // Imprime el byte del UID leido en hexadecimal
    variableUID += hexByte; // Almacena en String el byte del UID leido
  }
  return variableUID; // Retorna la variableUID ya convertida a mayúsculas
}


void sendMessage()
{
  Serial.println("\n");
  Serial.println("Se enviara un JSON a Node.js con la siguiente información: ");
  Serial.println(lecturaUID);

  // creat JSON message for Socket.IO (event)
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
          
  // add event namer
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

void receiveMessege(uint8_t* payload, size_t length){
  char* sptr = NULL;
  int id = strtol((char*)payload, &sptr, 10);

  USE_SERIAL.printf("[IOc] get event: %s id: %d\n", payload, id);

  if(id){
    payload = (uint8_t *)sptr;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if(err) {
    USE_SERIAL.print(F("deserializeJson() failed: "));
    USE_SERIAL.println(err.c_str());
    return;
  }

  // Acceder a los valores
  const char* nameEvent = doc[0];  // "UID"
  Serial.println(nameEvent);

  JsonObject persona = doc[1];
  //long Codigo = persona["Codigo"];
  //const char* UID_persona = persona["UID"];
  //const char* Nombre = persona["Nombre"];
  //int Ubicacion = persona["Ubicacion"];
  //const char* DatoAcademico = persona["DatoAcademico"];
  //int Estatus = persona["Estatus"];
  chapaStatus = persona["Estatus"];
}