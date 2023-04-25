void connect_to_wifi() {
  int status = WL_IDLE_STATUS;
  int i = 0;
  while (status != WL_CONNECTED && i < 3) {
    #ifdef DEBUGSERIAL
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(ssid.c_str());
    #endif
    status = WiFi.begin(ssid.c_str(), pass.c_str());
    delay(10000);
    i++;
  }
  #ifdef DEBUGSERIAL
  Serial.println("[WiFi] Connected");
  #endif
}

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

//receive mqtt messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if(strcmp(topic, "cas") == 0) {
    memset(cas,0,sizeof(cas));
    for (int i=0;i<length;i++) {
      cas[i] = (char)payload[i];
    }
  } else if(strcmp(topic, "meteostanice/teplota") == 0) {
    memset(ext_temp,0,sizeof(ext_temp));
    for (int i=0;i<length;i++) {
      ext_temp[i] = (char)payload[i];
    }
  } else if(strcmp(topic, "meteostanice/vlhkost") == 0) {
    memset(ext_humidity,0,sizeof(ext_humidity));
    for (int i=0;i<length;i++) {
      ext_humidity[i] = (char)payload[i];
    }
  } else if(strcmp(topic, "meteostanice/tlak") == 0) {
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
      mqttclient.subscribe("cas"); //time
      mqttclient.subscribe("meteostanice/teplota"); //temperature
      mqttclient.subscribe("meteostanice/vlhkost"); //humidity
      mqttclient.subscribe("meteostanice/tlak"); //atm. pressure
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
