void reconnect_wifi() {
  int status = WL_IDLE_STATUS;
  //WiFi.end();
  status = WiFi.begin(ssid.c_str(), pass.c_str());
   if (WiFi.status() == WL_NO_SHIELD) {
    #ifdef DEBUGSERIAL
    Serial.println("Communication with WiFi module failed!");
    #endif
    digitalWrite(LED_BUILTIN, HIGH);
    while (true);
  }
  int i = 0;
  status = WiFi.status();
  while (status != WL_CONNECTED && i < 3) {
    #ifdef DEBUGSERIAL
    Serial.print("[WiFi] Reconnecting to: ");
    Serial.println(ssid.c_str());
    #endif
    status = WiFi.begin(ssid.c_str(), pass.c_str());
    delay(10000);
    i++;
  }

  if (status == WL_CONNECTED) {
    #ifdef DEBUGSERIAL
    Serial.println("[WiFi] Successfully reconnected");
    #endif
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    #ifdef DEBUGSERIAL
    Serial.println("[WiFi] Reconnect attempt unsuccessful");
    #endif
  }
}

/*receive mqtt messages*/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  /*filter messages by topics and save to correct variables*/
  if(strcmp(topic, topic_time.c_str()) == 0) {
    memset(cas,0,sizeof(cas));
    for (int i=0;i<length;i++) {
      cas[i] = (char)payload[i];
    }
  } else if(strcmp(topic, topic_temperature.c_str()) == 0) {
    memset(ext_temp,0,sizeof(ext_temp));
    for (int i=0;i<length;i++) {
      ext_temp[i] = (char)payload[i];
    }
  } else if(strcmp(topic, topic_humidity.c_str()) == 0) {
    memset(ext_humidity,0,sizeof(ext_humidity));
    for (int i=0;i<length;i++) {
      ext_humidity[i] = (char)payload[i];
    }
  } else if(strcmp(topic, topic_pressure.c_str()) == 0) {
    memset(ext_pressure,0,sizeof(ext_pressure));
    for (int i=0;i<length;i++) {
      ext_pressure[i] = (char)payload[i];
    }
  }
}

void reconnect_mqtt() {
  int i = 0;
  while (!mqttclient.connected() && i < 3) {
    Serial.print("Attempting MQTT connection...");
    if (mqttclient.connect("wioterminal")) {
      Serial.println("connected");
      /*(re)subscribe*/
      mqttclient.subscribe(topic_time.c_str());
      mqttclient.subscribe(topic_temperature.c_str());
      mqttclient.subscribe(topic_humidity.c_str());
      mqttclient.subscribe(topic_pressure.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    i++;
  }
}

/*depending on CO2 level, light RGB with different colours*/
void rgb_indicate(int ppm) {
  if(ppm < 1000) {
    for(int i = 0;i<LED_COUNT;i++) {
      RGBLED.setPixelColor(i, 0,127,0);
    }
    RGBLED.show();
  } else if (ppm >= 1000 && ppm < 2000) {
    for(int i = 0;i<LED_COUNT;i++) {
      RGBLED.setPixelColor(i, 127,127,0);
    }
    RGBLED.show();
  } else {
    for(int i = 0;i<LED_COUNT;i++) {
      RGBLED.setPixelColor(i, 127,0,0);
    }
    RGBLED.show();
  }
}

/*button A (on the right) - toggle RGB LED indication*/
void buttonA() {
 if (rgb_indication) {
    for(int i = 0;i<LED_COUNT;i++) {
      RGBLED.setPixelColor(i,0,0,0);
    }
    RGBLED.show();
    rgb_indication = 0;
 } else {
    rgb_indicate(co2);
    rgb_indication = 1;
 }
}

/*button B (middle) - increase LCD brightness*/
void buttonB() {
  if(brightness <= 90 && millis() - last_brightness_change > 300) {
    brightness += 10;
    backLight.setBrightness(brightness);
    last_brightness_change = millis();
  }
}

/*button B (on the left) - decrease LCD brightness*/
void buttonC() {
  if(brightness >= 10 && millis() - last_brightness_change > 300) {
    brightness -= 10;
    backLight.setBrightness(brightness);
    last_brightness_change = millis();
  }
}