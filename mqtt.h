#ifndef MQTT_H
#define MQTT_H

#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>

#define MQTT_BASE_TOPIC       "coffee_scale"
#define MQTT_CMD_TOPIC        MQTT_BASE_TOPIC "/command"
#define MQTT_STATE_TOPIC      MQTT_BASE_TOPIC "/state"
#define MQTT_AVAILABLE_TOPIC  MQTT_BASE_TOPIC "/available"
#define MQTT_EVENT_TOPIC      MQTT_BASE_TOPIC "/event"

extern AsyncMqttClient mqttClient;

void initiateMqtt();
void handleMqtt();
void mqttPublish(const char* topic, const char* payload, bool retained = false);
void mqttPublishState(const char* key, const char* value);
void mqttPublishEvent(const char* event);
void mqttPublishEventJson(const char* event, float weight, float timeSec, const char* doseType, const char* stopReason,
                          float avgFlow = 0, float peakFlow = 0, float firstDrip = 0);

#endif // MQTT_H
