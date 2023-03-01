void connect_to_wifi() {
  int status = WL_IDLE_STATUS;
  int i = 0;
  while (status != WL_CONNECTED && i < 3) {
    #ifdef DEBUGSERIAL
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(ssid);
    #endif
    status = WiFi.begin(ssid, password);
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
  status = WiFi.begin(ssid, password);
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
    Serial.println(ssid);
    #endif
    status = WiFi.begin(ssid, password);
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
    for (int i=0;i<length;i++) {
      cas[i] = (char)payload[i];
    }
  } else if(strcmp(topic, "teplota") == 0) {
    for (int i=0;i<length;i++) {
      //implement temperature value parsing
    }
  } else if(strcmp(topic, "vlhkost") == 0) {
    for (int i=0;i<length;i++) {
    }
  } else if(strcmp(topic, "tlak") == 0) {
    for (int i=0;i<length;i++) {
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
      mqttclient.subscribe("teplota"); //temperature
      mqttclient.subscribe("vlhkost"); //humidity
      mqttclient.subscribe("tlak"); //atm. pressure
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
