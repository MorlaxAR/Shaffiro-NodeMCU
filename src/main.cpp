#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESP8266TrueRandom.h>

char* server = "192.168.22.6";
char* dataTopic;
char* actionTopic;
char* id;
unsigned long timeout;
unsigned long elapsedTime;
unsigned long initialTime;
int pin;
String mode;
String type;

StaticJsonDocument<200> jsonDocument;
JsonArray array;
WiFiClient wifiClient;
WiFiUDP Udp;
PubSubClient client(wifiClient);


void callback(char* topic, byte* payload, unsigned int length) {
  byte *p = new byte(length);
  memcpy(p,payload,length);
  int c = (int)p[0];
  if (c == 48) {
    digitalWrite(pin, LOW);
    }
  else if (c == 49) {
    digitalWrite(pin, HIGH);
    }
  }


void setup() {

  Serial.begin(115200);

// WiFi Settings
  const char* ssid     = "LSI-4"; // WIFI SSID
  const char* password = "lsi18lsi"; // WIFI PASSWORD
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Intentando conectarse a la red: ");
  Serial.print(ssid);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conexión exitosa!");
  Serial.println("Dirección IP: ");
  Serial.print(WiFi.localIP());
  Serial.println("");

  client.setServer(server, 1883);
  client.setCallback(callback);

// uuid 
  String uuidStr = "";
  byte uuidNumber[16];
  ESP8266TrueRandom.uuid(uuidNumber);
  uuidStr = ESP8266TrueRandom.uuidToString(uuidNumber);

  Serial.print("MAC: ");
  Serial.print(WiFi.macAddress());
  Serial.println("");

  Serial.print("uuid: ");
  Serial.print(uuidStr);
  Serial.println("");

// config
  String configStr = "";
  Serial.println("Archivo de configuración no encontrado, solicitando configuración");
  const unsigned int udpServerPort = 6789;
  const unsigned int udpLocalPort = 2500;
  Udp.begin(udpLocalPort);
  String bcastString = "{\"dispositivo\":{\"mac\":\"";
  bcastString += WiFi.macAddress();
  bcastString += "\",\"uuid\":\"";
  bcastString += "9862d20e-6c84-4887-9cb8-855693f6b9c7";
  //bcastString += uuidStr;
  bcastString += "\"}}";
  char bcastPacket[255] = "";
  bcastString.toCharArray(bcastPacket, 255);
  int configured = 0;
  while (!configured){
    delay(1000);
    Udp.beginPacket(server, udpServerPort);
    Serial.println(bcastPacket);
    Udp.write(bcastPacket);
    Udp.endPacket();
    Serial.println("Paquete de descubrimiento enviado!\n");
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      char incomingPacket[255];
      int len = Udp.read(incomingPacket, 255);
      if (len > 0){
        incomingPacket[len] = 0;
        Serial.printf("Recibido paquete de configuración con el contenido: %s\n", incomingPacket);
        configStr = incomingPacket;
        configured = applyConfig(configStr);
      }
    }
  }

  Serial.print("Configuración: ");
  Serial.print(configStr);
  Serial.println("");
}

int applyConfig(String configStr) {
  auto error = deserializeJson(jsonDocument, configStr);
  array = jsonDocument.as<JsonArray>();
  if(error) {
   Serial.println("deserializeJson() failed\n");
   return 0;
  }
  else {
    for(JsonVariant device : array) {
      pin = device["pin"];
      String modeString = device["mode"];
      String typeString = device["type"];
      String idString = device["id"];
      idString.toCharArray(id,255);
      client.connect(id);
      if (modeString.equals("OUTPUT")) {
        pinMode(pin, OUTPUT);
        String dataTopicString = "actuador/";
        dataTopicString += idString;
        dataTopicString += "/valor";
        dataTopicString.toCharArray(dataTopic, 255);
        String actionTopicString = "actuador/";
        actionTopicString += idString;
        actionTopicString += "/accion";
        actionTopicString.toCharArray(actionTopic, 255);
        client.subscribe(actionTopic);
      }
      if (modeString.equals("INPUT")) {
        pinMode(pin, INPUT_PULLUP);
        String dataTopicString = "sensor/";
        dataTopicString += idString;
        dataTopicString += "/valor";
        dataTopicString.toCharArray(dataTopic, 255);
        if (typeString.equals("proximidad")) {
          timeout = device["timeout"];
        }
      }
    }
    return 1;
  }
}


void loop() {
  if (mode.equals("INPUT")) {
    if (type.equals("proximidad")) {
      if (!digitalRead(pin)) {
        initialTime = millis();
        while (timeout > elapsedTime) {
          client.publish(dataTopic,"1");
          elapsedTime =  (millis() - initialTime);
          delay(100);
        }
      }
      else {
        client.publish(dataTopic,"0");
      }
    }
  }
}
