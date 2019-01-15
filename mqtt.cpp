// **********************************************************************************
// MQTT source file for remora project
// **********************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend but please abide with the CC-BY-SA license:
// http://creativecommons.org/licenses/by-sa/4.0/
//
// Written by bronco0 (https://github.com/bronco0/remora_soft)
//
// History : 08/01/2019 : First release
//
// All text above must be included in any redistribution.
// **********************************************************************************

#include "mqtt.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
int nbRestart = 0;

void connectToMqtt() {
  Debugln("Connection au broker MQTT...");
  initMqtt();
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Debugln("Connecté au broker MQTT");
  Debug("Session : ");
  Debugln(sessionPresent);
  if (sessionPresent)
    nbRestart = 0;

  // subscribe au topic setFP
  uint16_t packetIdSub = mqttClient.subscribe(MQTT_TOPIC_SET, 2);
  Debug("Subscribing at QoS 2, packetId: ");
  Debugln(packetIdSub);

  mqttClient.publish(MQTT_TOPIC_FP, 2, true, "{\"FP\":\"UP\"}");
  mqttClient.publish(MQTT_TOPIC_RELAIS, 2, true, "{\"RELAIS\":\"UP\"}");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Debugln("Déconnection du broker MQTT.");

  if (WiFi.isConnected()) {
    if (nbRestart < 2)
      mqttReconnectTimer.once(2, connectToMqtt);
    else if (nbRestart < 5)
      mqttReconnectTimer.once(10, connectToMqtt);
    else if (nbRestart < 10)
      mqttReconnectTimer.once(30, connectToMqtt);
    else
      mqttReconnectTimer.once(60, connectToMqtt);
  }
  nbRestart++;
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Debugln("Subscribe acknowledged.");
  Debug("  packetId: ");
  Debugln(packetId);
  Debug("  qos: ");
  Debugln(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Debugln("Unsubscribe acknowledged.");
  Debug("  packetId: ");
  Debugln(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Debugln("Publish received.");
  Debug("  topic: ");
  Debugln(topic);
  Debug("  qos: ");
  Debugln(properties.qos);
  Debug("  dup: ");
  Debugln(properties.dup);
  Debug("  retain: ");
  Debugln(properties.retain);
  Debug("  len: ");
  Debugln(len);
  Debug("  index: ");
  Debugln(index);
  Debug("  total: ");
  Debugln(total);

  String message = String(payload);

  if ( len == 2 && message.startsWith("FP") ) {
    String message = String("FP=" + String(etatFP));
    char message_send[] = "";
    message.toCharArray(message_send, message.length() + 1);
    Debug("message_send = ");
    Debugln(message_send);
    if (mqttClient.publish(MQTT_TOPIC_FP, 2, true, message_send)  == 0) {
      Debugf("Mqtt : Erreur publish FP2\n");
    }
  }
  else if (len == 5 && message.startsWith("FP=")) {
    message.remove(len);
    message.remove(0, 3);
    Debug("message = ");
    Debugln(message);
    setfp(message);
  }
  else if (len == 11 && message.startsWith("FPS=")) {
    message.remove(len);
    message.remove(0, 4);
    Debug("message = ");
    Debugln(message);
    fp(message);
  }
  else if (len == 3 && message.startsWith("R=")) {
    message.remove(len);
    message.remove(0, 2);
    Debug("message = ");
    Debugln(message);
    fnct_relais(message);
  }
  else {
    Debugln("Mqtt: Bad payload");
  }
}

void onMqttPublish(uint16_t packetId) {
  Debugln("Publish acknowledged.");
  Debug("  packetId: ");
  Debugln(packetId);
}

void initMqtt(void) {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  #ifdef MOD_MQTT
    #if(defined MQTT_HOST && defined MQTT_PORT)
      mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    #else
      if (config.mqtt.host != "" && config.mqtt.port > 0) {
        mqttClient.setServer(config.mqtt.host, config.mqtt.port);
      }
    #endif
    if (config.mqtt.user != "" && config.mqtt.password != "") {
      mqttClient.setCredentials(config.mqtt.user, config.mqtt.password);
    }
    #if ASYNC_TCP_SSL_ENABLED
      if (config.mqtt.protocol == "mqtts") {
        mqttClient.setSecure(true);
      }
    #endif
  #endif
}
