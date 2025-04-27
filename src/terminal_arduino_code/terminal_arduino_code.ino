/*
   IoT weather and CO2 terminal 
   Jiří Vavřík
   Derived from project by: Salman Faris
   Date: 26/11/2023
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
#include <Adafruit_NeoPixel.h>
#define LED_PIN 1
#define LED_COUNT 3
Adafruit_NeoPixel RGBLED = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB);

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft);  //sprite
static LCDBackLight backLight;

File file_serveraddr, file_wifi_ssid, file_wifi_pass, file_topic_time, file_topic_temperature, file_topic_humidity, file_topic_pressure, file_mqtt_port;
std::string server, ssid, pass, topic_time, topic_temperature, topic_humidity, topic_pressure, mqtt_port_str;
int mqtt_port;

int brightness = 10;
int last_brightness_change = 0;
int rgb_indication = 1;
int wifi_disable = 0;
int mqtt_disable = 0;
int scd30_fully_ready = 0;

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
void buttonA();
void buttonB();
void buttonC();
void rgb_indicate(int);

void setup() {
  /*configure buttons interrupts*/
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_A), buttonA, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_B), buttonB, FALLING);
  attachInterrupt(digitalPinToInterrupt(WIO_KEY_C), buttonC, FALLING);

  Serial.begin(115200);
  Serial.println("Starting...");
  RGBLED.begin();
  RGBLED.show(); // Initialize all pixels to 'off'

  tft.begin();
  tft.setRotation(3);
  backLight.initialize();

  /*draw the static shapes on LCD*/
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
  tft.drawRoundRect(5, 60, (tft.width() / 2) - 20 , tft.height() - 65 , 10, TFT_WHITE);
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

  backLight.setBrightness(brightness);
  
  /*init the temp & humidity SHT31 sensor*/
  Serial.println("SHT31 test");
  if (!sht.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  /*make sure to disable SHT31's heater*/
  Serial.print("Heater Enabled State: ");
  if (sht.isHeaterEnabled())
    Serial.println("ENABLED");
  else
    Serial.println("DISABLED");

  /*Try to initialize SCD30 CO2 sensor*/
  if (!scd30.begin()) { /*begin function also calls _init() that starts continuous measurement and sets interval to 2 s*/
    Serial.println("Failed to find SCD30 chip");
    //while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");

  /*initialize SD card and read all required parameters from it*/
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("SD card initialization failed!");
    /*while (1);*/
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

  /*configure MQTT*/
  if(!server.empty() && !mqtt_port_str.empty()) {
    mqttclient.setServer(server.c_str(), mqtt_port);
    mqttclient.setCallback(callback);
    delay(5000);
  } else {
    mqtt_disable = 1; /*mqtt communication will be turned off*/
    Serial.println("MQTT server address or port could nto be read from SD. MQTT communication will be turned off.");
  }

  if(!ssid.empty() && !pass.empty()) {
    WiFi.begin(ssid.c_str(), pass.c_str()); //quick wifi connect attempt
  } else {
    wifi_disable = 1;
    Serial.println("Wi-Fi SSID or password could nto be read from SD. Wi-Fi communication will be turned off.");
  }
}

void loop() {
  char text_line1[18], text_line2[12];/*first and second line of text zo be drawn to the "outside weather" part of the display*/

  /*check if SCD30 has made a measurement*/
  if (scd30.dataReady()){
    Serial.println("Data available!");
    if (!scd30.read()){
      Serial.println("Error reading sensor data");
      return;
    }
    co2 = scd30.CO2;
    Serial.print("CO2: ");
    Serial.print(co2, 3);
    Serial.println(" ppm");
    Serial.println("");
    /*if enabled, light RGB LED according to CO2 level*/
    if(rgb_indication && co2){
      rgb_indicate(co2);
    }
  } else {
    Serial.println("No data");
  }
  
  float t = sht.readTemperature();
  float h = sht.readHumidity();
  
  /*because first reading from SCD30 gives co2 value of zero, we wait for first nonzero reading before setting the interval from 2 to desired higher value*/
  if(!scd30_fully_ready) {
    if(co2) {
      if (!scd30.setMeasurementInterval(30)){
        Serial.println("Failed to set measurement interval");
      } else {
        scd30_fully_ready = 1;
      }
    }
  }

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

  /*draw measurements to the LCD*/
  //inside temperature
  spr.createSprite(65, 30);
  spr.fillSprite(TFT_BLACK);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
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

  /*check WiFi connectivity*/
  if(!wifi_disable) {
    if(WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, HIGH);
      reconnect_wifi();
    }
  }

  if(!mqtt_disable) {
    if (!mqttclient.connected()) {
      reconnect_mqtt();
    }
    mqttclient.loop(); //documentation: This should be called regularly to allow the client to process incoming messages and maintain its connection to the server.
  }
  
  /*display outside conditions to the LCD*/
  spr.createSprite(145, 46);
  spr.setFreeFont(&FreeSansBoldOblique9pt7b);
  spr.setTextColor(TFT_WHITE);
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

  /*draw current time to the LCD (time is received by MQTT)*/
  Serial.print("promenna cas: ");Serial.println(cas);
  spr.createSprite(65, 30);
  spr.setFreeFont(&FreeSansBoldOblique12pt7b);
  spr.setTextColor(TFT_WHITE);
  spr.drawString(cas, 0 , 0, 1);
  spr.pushSprite(((tft.width() / 2) + (tft.width() / 2) / 2), (tft.height() / 2) + 67);
  spr.deleteSprite();
  
  delay(1000);
}
