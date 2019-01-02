// include MQTT library from marvinroger
#include <AsyncMqttClient.h>
#include "remora.h"

#define MQTT_TOPIC_BASE "remora/"

// under MQTT_TOPIC_SET  we are reading subtopics to set fp & relai
#define MQTT_TOPIC_SET  MQTT_TOPIC_BASE "set"

// under MQTT_TOPIC_FP we are send fp informations
#define MQTT_TOPIC_FP     MQTT_TOPIC_BASE "fp"

// under MQTT_TOPIC_FP we are send fp informations
#define MQTT_TOPIC_RELAIS MQTT_TOPIC_BASE "relais"

// Under MQTT_TOPIC_TINFO we are send teleinformation
#define MQTT_TOPIC_TINFO  MQTT_TOPIC_BASE "tinfo"


void connectToMqtt(void);
void onMqttConnect(bool);
void onMqttDisconnect(AsyncMqttClientDisconnectReason);
void onMqttSubscribe(uint16_t, uint8_t);
void onMqttUnsubscribe(uint16_t packetId);
void onMqttMessage(char*, char*, AsyncMqttClientMessageProperties, size_t, size_t, size_t);
void onMqttPublish(uint16_t);
void initMqtt(void);
