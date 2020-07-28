#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <ESP8266TrueRandom.h>
#include <Wire.h>
#include <BH1750.h>

//Global Variables
char* server = "35.208.43.150"; //Server IP
int pinDispositivo = 2; //Pin Relay
const char* dataTopic = "";
const char* tipoDispositivo = "";
const char* nombreDispositivo = "";

BH1750 lightMeter;
StaticJsonDocument<200> configJson;
WiFiClient wifiClient;
WiFiUDP Udp;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje:");
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  mensaje.trim();
  Serial.println(mensaje);
  if (mensaje.equals("1")){
    Serial.println("Encendiendo Actuador!");
    digitalWrite(pinDispositivo, LOW);
  }
  else if  (mensaje.equals("0")){
    Serial.println("Apagando Actuador!");
    digitalWrite(pinDispositivo, HIGH);
  }
}

void setup() {
  Serial.begin(115200);

  // WiFi Settings
  const char* ssid     = "SSID"; // WIFI SSID
  const char* password = "PASSWORD"; // WIFI PASSWORD
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

  // uuid File
  SPIFFS.begin();
  //SPIFFS.format();
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

  // Config File
  String configStr = "";
  if (!SPIFFS.exists("/config.txt")) {
    Serial.println("Archivo de configuración no encontrado, solicitando configuración");
    const unsigned int udpServerPort = 6789;
    const unsigned int udpLocalPort = 2500; //Debe ser único entre dispositivos detrás de NAT
    uuidStr.trim();
    String localPortStr = String(udpLocalPort); 
    Udp.begin(udpLocalPort);
    String bcastString = "{\"dispositivo\":{\"mac\":\"";
    bcastString += WiFi.macAddress();
    bcastString += "\",\"uuid\":\"";
    bcastString += uuidStr;
    bcastString += "\",\"puerto\":";
    bcastString += localPortStr;
    bcastString += "}}";
    char bcastPacket[255] = "";
    bcastString.toCharArray(bcastPacket, 255);
    int configured = 0;
    while (!configured){
      delay(1000);
      Udp.beginPacket(server, udpServerPort);
      Serial.println(bcastPacket);
      Udp.write(bcastPacket);
      Udp.endPacket();
      Serial.println("Paquete de descubrimiento enviado!");
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

  //JSON Parse
  DeserializationError err = deserializeJson(configJson, configStr);
  if(err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
  }
  else {
    nombreDispositivo = configJson["nombreDispositivo"];
    tipoDispositivo = configJson["tipoDispositivo"];
    dataTopic = configJson["topic"];
    Serial.println("Json deserializado con exito\n");
  }

  // Configuración MQTT Broker
  Serial.println("Conectandose al MQTT Broker");
  client.setServer(server, 1883);
  while (!client.connected()) {
    Serial.println("Conectando al Broker MQTT...");
    if (client.connect(nombreDispositivo)) {
      Serial.println("Conexión Exitosa");  
    } 
    else {
      Serial.print("Conexión falló con el estado ");
      Serial.print(client.state());
      Serial.println();
      delay(2000);
    }
  }

  //Configuración Dispositivo
  if (strcmp(tipoDispositivo,"SENSOR") == 0) {
    Serial.println("tipoDispositivo es SENSOR");
    Wire.begin();
    if (lightMeter.begin()) {
      Serial.println("Sensor de Luz inicializado");
    }
    else {
      Serial.println("Error inicializando Sensor de Luz");
    }
  }
  else if (strcmp(tipoDispositivo,"ACTUADOR") == 0) {
    Serial.println("tipoDispositivo es ACTUADOR");
    client.setCallback(callback);
    pinMode(pinDispositivo, OUTPUT);
  }
  else {
    Serial.println("tipoDispositivo no reconocido");
  }
}

void loop() {
  if (strcmp(tipoDispositivo,"SENSOR") == 0) {
    char* luxStr = "";
    uint16_t lux = lightMeter.readLightLevel();
    sprintf(luxStr, "%u", lux);
    Serial.printf("Light: %s lx\n",luxStr);
    client.publish(dataTopic,luxStr);
    delay(1000);
  }
  client.loop();
}
