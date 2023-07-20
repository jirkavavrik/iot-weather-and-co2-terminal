/*
   IoT weather and CO2 terminal 
   Jiří Vavřík
   Derived from project by: Salman Faris
   Date: 28/01/2023
*/
#define DEBUGSERIAL
#include <Adafruit_SHT31.h>
#include <PubSubClient.h>
#include <rpcWiFiClientSecure.h>
#include <Adafruit_SCD30.h>
#include <SPI.h>
#include <Seeed_FS.h>
#include "SD/Seeed_SD.h"
#include <string>
#include "lcd_backlight.hpp"
#include <TFT_eSPI.h>
#include <Wire.h>

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);  //sprite
static LCDBackLight backLight;

File file_serveraddr, file_wifi_ssid, file_wifi_pass, file_topic_time, file_topic_temperature, file_topic_humidity, file_topic_pressure, file_mqtt_port;
std::string server, ssid, pass, topic_time, topic_temperature, topic_humidity, topic_pressure, mqtt_port_str;
int mqtt_port;

Adafruit_SCD30  scd30;
unsigned int co2;
Adafruit_SHT31 sht = Adafruit_SHT31();

WiFiClient wificlient;
PubSubClient mqttclient(wificlient);
char cas[10], ext_temp[10], ext_humidity[10], ext_pressure[10];

void connect_to_wifi();
void reconnect_wifi();
void callback(char*, byte*, unsigned int);
void reconnect_mqtt();

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  backLight.initialize();

  //Header
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(&FreeSansBoldOblique18pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Monitor vzduchu", 18, 10 , 1);
  
  //Line
  for (int8_t line_index = 0; line_index < 5 ; line_index++) {
    tft.drawLine(0, 50 + line_index, tft.width(), 50 + line_index, TFT_GREEN);
  }
  //temperature & humidity rectangle
  tft.drawRoundRect(5, 60, (tft.width() / 2) - 20 , tft.height() - 65 , 10, TFT_WHITE); // L1

  tft.setFreeFont(&FreeSansBoldOblique12pt7b);
  tft.setTextColor(TFT_RED);
  tft.drawString("Teplota", 7 , 65 , 1);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("st. C", 80, 108, 1);

  tft.setFreeFont(&FreeSansBoldOblique12pt7b);
  tft.setTextColor(TFT_RED);
  tft.drawString("Vlhkost", 7 , 150 , 1);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(" %", 80, 193, 1);

  //outside weather rectangle
  tft.drawRoundRect((tft.width() / 2) - 10  , 60, (tft.width() / 2) +5, (tft.height() - 65) / 2, 10, TFT_BLUE); // s1

  tft.setFreeFont(&FreeSansBoldOblique9pt7b);
  tft.setTextColor(TFT_RED) ;
  tft.drawString("Venku je:", (tft.width() / 2) - 1  , 70 , 1); // Print the test text in the custom font

  //co2 rectangle
  tft.drawRoundRect((tft.width() / 2) - 10 , (tft.height() / 2) + 30, (tft.width() / 2) / 2 , (tft.height() - 65) / 2 , 10, TFT_WHITE); // s3

  tft.setFreeFont(&FreeSansBoldOblique9pt7b);
  tft.setTextColor(TFT_RED) ;
  tft.drawString("CO2", (tft.width() / 2) - 1 , (tft.height() / 2) + 40 , 1); // Print the test text in the custom font
  tft.setTextColor(TFT_GREEN);
  tft.drawString("ppm", (tft.width() / 2) + 30, (tft.height() / 2) + 90, 1);


  //time rectangle
  tft.drawRoundRect(((tft.width() / 2) + (tft.width() / 2) / 2) - 5  , (tft.height() / 2) + 30, (tft.width() / 2) / 2 , (tft.height() - 65) / 2 , 10, TFT_BLUE); // s4

  tft.setFreeFont(&FreeSansBoldOblique9pt7b);
  tft.setTextColor(TFT_RED) ;
  tft.drawString("Cas:", ((tft.width() / 2) + (tft.width() / 2) / 2)   , (tft.height() / 2) + 40 , 1); // Print the test text in the custom font
  tft.setTextColor(TFT_GREEN);
  //tft.drawString("ppm", ((tft.width() / 2) + (tft.width() / 2) / 2) + 30 , (tft.height() / 2) + 90, 1);

  backLight.setBrightness(20);
   
  Serial.println("SHT31 test");
  if (!sht.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  Serial.print("Heater Enabled State: ");
  if (sht.isHeaterEnabled())
    Serial.println("ENABLED");
  else
    Serial.println("DISABLED");

  // Try to initialize
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    //while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");

  if (!scd30.setMeasurementInterval(30)){
    Serial.println("Failed to set measurement interval");
    /*while(1){ delay(10);}*/
  }
  Serial.print("Measurement Interval: "); 
  Serial.print(scd30.getMeasurementInterval()); 
  Serial.println(" seconds");

  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  
  file_serveraddr = SD.open("serveraddr.txt", FILE_READ);
  if (file_serveraddr) {
    while (file_serveraddr.available()) {
      server += file_serveraddr.read();
    }
    file_serveraddr.close();
  } else {
    Serial.println("error opening serveraddr.txt");
  }

  file_wifi_ssid = SD.open("wifi_ssid.txt", FILE_READ);
  if (file_wifi_ssid) {
    while (file_wifi_ssid.available()) {
      ssid += file_wifi_ssid.read();
    }
    file_wifi_ssid.close();
  } else {
    Serial.println("error opening wifi_ssid.txt");
  }

  file_wifi_pass = SD.open("wifi_pass.txt", FILE_READ);
  if (file_wifi_pass) {
    while (file_wifi_pass.available()) {
      pass += file_wifi_pass.read();
    }
    file_wifi_pass.close();
  } else {
    Serial.println("error opening wifi_pass.txt");
  }

  file_mqtt_port = SD.open("mqtt_port.txt", FILE_READ);
  if (file_mqtt_port) {
    while (file_mqtt_port.available()) {
      mqtt_port_str += file_mqtt_port.read();
    }
    file_mqtt_port.close();
    mqtt_port = std::stoi(mqtt_port_str,nullptr,10);
  } else {
    Serial.println("error opening mqtt_port.txt");
  }

  file_topic_time = SD.open("topic_time.txt", FILE_READ);
  if (file_topic_time) {
    while (file_topic_time.available()) {
      topic_time += file_topic_time.read();
    }
    file_topic_time.close();
  } else {
    Serial.println("error opening topic_time.txt");
  }

  file_topic_temperature = SD.open("topic_temperature.txt", FILE_READ);
  if (file_topic_temperature) {
    while (file_topic_temperature.available()) {
      topic_temperature += file_topic_temperature.read();
    }
    file_topic_temperature.close();
  } else {
    Serial.println("error opening topic_temperature.txt");
  }

  file_topic_humidity = SD.open("topic_humidity.txt", FILE_READ);
  if (file_topic_humidity) {
    while (file_topic_humidity.available()) {
      topic_humidity += file_topic_humidity.read();
    }
    file_topic_humidity.close();
  } else {
    Serial.println("error opening topic_humidity.txt");
  }

  file_topic_pressure = SD.open("topic_pressure.txt", FILE_READ);
  if (file_topic_pressure) {
    while (file_topic_pressure.available()) {
      topic_pressure += file_topic_pressure.read();
    }
    file_topic_pressure.close();
  } else {
    Serial.println("error opening topic_pressure.txt");
  }

  mqttclient.setServer(server.c_str(), mqtt_port);
  mqttclient.setCallback(callback);
  delay(5000);

  WiFi.begin(ssid.c_str(), pass.c_str()); //quick wifi connect attempt
  //connect_to_wifi();
}

void loop() {
  char text_line1[18], text_line2[12];
  if (scd30.dataReady()){
    Serial.println("Data available!");
    if (!scd30.read()){ Serial.println("Error reading sensor data"); return; }
    
    Serial.print("CO2: ");
    Serial.print(scd30.CO2, 3);
    Serial.println(" ppm");
    Serial.println("");
  } else {
    Serial.println("No data");
  }
  
  float t = sht.readTemperature();
  float h = sht.readHumidity();
  int co2 = scd30.CO2;

  if (! isnan(t)) {  // check if 'is not a number'
    Serial.print("Temp *C = "); Serial.print(t); Serial.print("\t\t");
  } else { 
    Serial.println("Failed to read temperature");
  }
  
  if (! isnan(h)) {  // check if 'is not a number'
    Serial.print("Hum. % = "); Serial.println(h);
  } else { 
    Serial.println("Failed to read humidity");
  }

  delay(1000);

  //inside temperature
  spr.createSprite(65, 30);
  spr.fillSprite(TFT_BLACK);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
  //spr.drawNumber(t, 0, 0, 1);
  spr.drawFloat(t,2, 0, 0, 1);
  spr.pushSprite(15, 100);
  spr.deleteSprite();

  //inside humidity
  spr.createSprite(65, 30);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
  //spr.drawNumber(h, 0, 0, 1);
  spr.drawFloat(h, 2, 0, 0, 1);
  spr.setTextColor(TFT_GREEN);
  spr.pushSprite(15, 185);
  spr.deleteSprite();

  //co2
  spr.createSprite(60, 30);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
  spr.drawNumber(co2, 0, 0, 1);
  spr.pushSprite((tft.width() / 2) - 1, (tft.height() / 2) + 67);
  spr.deleteSprite();

  if(WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, HIGH);
      reconnect_wifi();
  }

  if (!mqttclient.connected()) {
    reconnect_mqtt();
  }
  mqttclient.loop(); //dokumentace: This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
  
  //weather outside
  spr.createSprite(145, 46);//spr.createSprite(30, 30);
  spr.setFreeFont(&FreeSansBoldOblique9pt7b);
  spr.setTextColor(TFT_WHITE);
  //spr.drawNumber(t, 0, 0, 1);
  sprintf(text_line1,"%s%s%s%s", ext_temp, " C, ", ext_humidity, " %");
  sprintf(text_line2,"%s%s", ext_pressure, " hPa");
  Serial.print("text line 1: ");
  Serial.println(text_line1);
  Serial.print("text line 2: ");
  Serial.println(text_line2);
  spr.drawString(text_line1,0,0,1);
  spr.drawString(text_line2,0,22,1);
  spr.setTextColor(TFT_GREEN);
  spr.pushSprite((tft.width() / 2) - 1, 100);
  spr.deleteSprite();

  //time
  Serial.print("promenna cas: ");Serial.println(cas);
  spr.createSprite(65, 30);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
  spr.drawString(cas, 0 , 0, 1);
  spr.pushSprite(((tft.width() / 2) + (tft.width() / 2) / 2), (tft.height() / 2) + 67);
  spr.deleteSprite();
  
  delay(1000);
}
