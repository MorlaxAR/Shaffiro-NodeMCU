#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266TrueRandom.h>

char* server = "192.168.22.6";

StaticJsonDocument<200> jsonDocument;
JsonArray array;
WiFiClient wifiClient;
WiFiUDP Udp;
PubSubClient client(wifiClient);

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


// uuid 
  SPIFFS.begin();
  SPIFFS.format();
  String uuidStr = "";
  if (!SPIFFS.exists("/uuid.txt")) {
    Serial.println("Archivo /uuid.txt no encontrado, generando uno nuevo.");
    byte uuidNumber[16];
    ESP8266TrueRandom.uuid(uuidNumber);
    
    uuidStr = ESP8266TrueRandom.uuidToString(uuidNumber);
    File uuid = SPIFFS.open("/uuid.txt","w");
    uuid.println(uuidStr);
    uuid.close();
  }
  else {
    File uuid = SPIFFS.open("/uuid.txt","r");
    while (uuid.available()) {
    uuidStr += char(uuid.read());
    }
  }

  Serial.print("MAC: ");
  Serial.print(WiFi.macAddress());
  Serial.println("");

  Serial.print("uuid: ");
  Serial.print(uuidStr);
  Serial.println("");

// config
  String configStr = "";
  if (!SPIFFS.exists("/config.txt")) {
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
          File config = SPIFFS.open("/config.txt","w");
          config.println(incomingPacket);
          config.close();
          configured = 1;
        }
      }
    }
  }
  else {
    File config = SPIFFS.open("/config.txt","r");
    while (config.available()) {
    configStr += char(config.read());
    }
  }
  Serial.print("Configuración: ");
  Serial.print(configStr);
  Serial.println("");
}


void loop() {
  // put your main code here, to run repeatedly:
}
