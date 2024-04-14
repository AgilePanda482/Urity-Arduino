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
const uint8_t RST_PIN = 27;
const uint8_t SS_PIN = 5;

//Chapa Pin
const uint8_t chapa = 13;

//Piezo Pin
//const uint8_t piezo = 33

//Variables
String lecturaUID = "";
bool rfidStatus = false;
bool chapaStatus = false;
unsigned long beforeRead = 0;
unsigned long beforeDoor = 0;


//Constantes
const int TiempoEsperaWifi = 5000;
const long chapaTime = 5000;
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
String almacenarUID();
//void sendMessage(String dataSend, String eventName, String dataName);
JsonDocument receiveMessage(uint8_t* payload, size_t length);

//Serial mode
#define USE_SERIAL Serial

void setup() {
  USE_SERIAL.begin(115200);
	USE_SERIAL.setDebugOutput(true);

  //Internet Conexions
  wifiMulti.addAP(ssid_1, password_1);
  wifiMulti.addAP(ssid_2, password_2);
  wifiMulti.addAP(ssid_3, password_3);
  WiFi.mode(WIFI_STA);

  Serial.print("Conectando a Wifi...");
  
  int conecctionTry = 0;
  while (wifiMulti.run(TiempoEsperaWifi) != WL_CONNECTED && conecctionTry < 10){
    USE_SERIAL.print(".");
    conecctionTry++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    USE_SERIAL.println("Conectado exitosamente");
    USE_SERIAL.print("SSID: ");
    USE_SERIAL.print(WiFi.SSID());
    USE_SERIAL.print("IP: ");
    USE_SERIAL.println(WiFi.localIP());
  }else{
    USE_SERIAL.println("No se pudo conectar a Wifi, verifique sus credenciales");
    return;
  }
  
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

  USE_SERIAL.println("Setup Done");
}

void loop() {
  socketIO.loop();

  if (chapaStatus) {
    digitalWrite(chapa, HIGH);
  }

  readRFID();

  unsigned long currentMillis = millis();
  if (currentMillis - beforeDoor >= chapaTime){
    digitalWrite(chapa, LOW);
    chapaStatus = false;
    mfrc522.PCD_Init();
    beforeDoor = currentMillis;
  }
}

/*------------FUNCTIONS---------------*/

//WebsocketIO function
void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
  char* eventName = (char*)payload;
  JsonDocument docJsonToBack;
  
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
    {
      if(strstr(eventName, "sendDatafromUID") != nullptr ) {
        bool locationStudent;
        JsonDocument docJson = receiveMessage(payload, length);
        JsonObject jsonLocalizacion = docJson[1];

        if(jsonLocalizacion["UID"] == 0){
          return;
        }

        chapaStatus = jsonLocalizacion["estadoInstitucional"];
        locationStudent = jsonLocalizacion["localizacionAlumno"]; 

        jsonLocalizacion["localizacionAlumno"] ? locationStudent = false : locationStudent = true;

        /*-----------Prepare Json-------------*/

        JsonArray array = docJsonToBack.to<JsonArray>();

        //event name for socket.io       
        array.add("changeStatus");

        //parameters for json
        JsonObject parameters = array.createNestedObject();
        parameters["UID"] = jsonLocalizacion["UIDTarjeta"];
        parameters["localizacionAlumno"] = locationStudent;

        sendMessage(docJsonToBack);
        break;
      } else if (strstr(eventName, "verifyUID") != nullptr ) {
        Serial.println("Si entre");
        JsonDocument docJson = receiveMessage(payload, length);
        JsonObject jsonVerify = docJson[1];

        //true || false
        rfidStatus = jsonVerify["verify"];
      }
    }
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
  String stringToBack = "";

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    if (millis() - beforeRead > intervalRFID){
      beforeRead = millis();
      lecturaUID = almacenarUID();
        
      JsonDocument UIDJson;
      JsonArray array = UIDJson.to<JsonArray>();

      rfidStatus ? stringToBack = "verifyUIDFromArduino" : stringToBack = "readUID";

      //event name for socket.io       
      array.add(stringToBack);

      //parameters for json
      JsonObject parameters = array.createNestedObject();
      parameters["UID"] = lecturaUID;

      sendMessage(UIDJson);
      lecturaUID = "";
      mfrc522.PICC_HaltA();
      stringToBack = "";
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

void sendMessage(JsonDocument doc) {
  // creat JSON message for Socket.IO (event)
  /*JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
          
  // add event namer
  // Hint: socket.on('event_name', ....
  array.add(eventName);

  // add payload (parameters) for the event
  JsonObject parameters = array.createNestedObject();
  parameters[dataName] = dataSend;*/

  // JSON to String (serializion)
  String output;
  serializeJson(doc, output);

  Serial.println("\nSe enviara un JSON a Node.js con la siguiente información: ");
  Serial.println(output);

  // Send event        
  socketIO.sendEVENT(output);

  // Print JSON for debugging
  //Serial.println(output);
}

JsonDocument receiveMessage(uint8_t* payload, size_t length){
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
    throw std::runtime_error("Error deserializing JSON");
  }

  return doc;
}