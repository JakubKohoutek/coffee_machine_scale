#include <credentials.h>
#include "mqtt.h"
#include "log.h"

AsyncMqttClient mqttClient;

static const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
static const unsigned long MQTT_MAX_RECONNECT_INTERVAL = 300000;
// Allow the TCP stack's SYN retries to finish before starting another attempt.
static const unsigned long MQTT_CONNECT_TIMEOUT = 120000;
static String mqttClientId;
static unsigned long lastMqttReconnectAttempt = 0;
static unsigned long mqttReconnectInterval = MQTT_RECONNECT_INTERVAL;
static unsigned long mqttConnectStarted = 0;
static bool mqttConnecting = false;
static bool mqttRetryScheduled = false;
static bool mqttConnectedEvent = false;
static bool mqttDisconnectedEvent = false;
static int mqttDisconnectReason = 0;
static char pendingCommand[128];
static size_t commandBytes = 0;
static bool commandReady = false;
static bool commandDropped = false;

extern void handleCommand(const String& cmd);
extern void publishAllStates();

/** Schedule one backoff per failed attempt, including a forced-close callback. */
void scheduleMqttRetry() {
    if (mqttRetryScheduled) return;
    mqttRetryScheduled = true;
    lastMqttReconnectAttempt = millis();
    mqttReconnectInterval = min(mqttReconnectInterval * 2, MQTT_MAX_RECONNECT_INTERVAL);
}

/** Assemble a bounded command; execute it from the main loop, not a TCP callback. */
void mqttCallback(char* topic, char* payload, AsyncMqttClientMessageProperties,
                  size_t length, size_t index, size_t total) {
    if (strcmp(topic, MQTT_CMD_TOPIC) != 0) return;
    if (index == 0) commandBytes = 0;
    if (commandReady || total == 0 || total >= sizeof(pendingCommand) ||
        index != commandBytes || index > total || length > total - index) {
        commandDropped = true;
        return;
    }
    memcpy(pendingCommand + index, payload, length);
    commandBytes += length;
    if (commandBytes == total) {
        pendingCommand[total] = '\0';
        commandReady = true;
    }
}

/** Configure asynchronous MQTT without waiting for Wi-Fi, DNS, TCP or CONNACK. */
void initiateMqtt() {
    mqttClientId = "coffee_scale_" + String(ESP.getChipId(), HEX);
    mqttClient.setClientId(mqttClientId.c_str());
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    #ifdef MQTT_USER
        mqttClient.setCredentials(MQTT_USER, MQTT_PASS);
    #endif
    mqttClient.setWill(MQTT_AVAILABLE_TOPIC, 0, true, "offline");
    mqttClient.onConnect([](bool) {
        mqttConnecting = false;
        mqttConnectedEvent = true;
    });
    mqttClient.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
        mqttConnecting = false;
        mqttDisconnectReason = static_cast<int>(reason);
        mqttDisconnectedEvent = true;
    });
    mqttClient.onMessage(mqttCallback);
    lastMqttReconnectAttempt = millis() - MQTT_RECONNECT_INTERVAL;
    logMessage("[MQTT] Async client ready");
}

/** Service deferred events and bounded reconnect attempts without blocking UI input. */
void handleMqtt() {
    if (mqttDisconnectedEvent) {
        mqttDisconnectedEvent = false;
        scheduleMqttRetry();
        logMessage(String("[MQTT] Disconnected, reason=") + mqttDisconnectReason +
                   ", attempt/connection age_ms=" + String(millis() - mqttConnectStarted) +
                   ", retry_ms=" + String(mqttReconnectInterval));
    }
    if (mqttConnectedEvent) {
        mqttConnectedEvent = false;
        if (mqttClient.connected()) {
            mqttReconnectInterval = MQTT_RECONNECT_INTERVAL;
            mqttRetryScheduled = false;
            mqttClient.subscribe(MQTT_CMD_TOPIC, 0);
            publishAllStates();
            logMessage(String("[MQTT] Connected, elapsed_ms=") + String(millis() - mqttConnectStarted));
        }
    }
    if (commandReady) {
        String command(pendingCommand);
        commandReady = false;
        logMessage("[MQTT] Command: " + command);
        handleCommand(command);
    }
    if (commandDropped) {
        commandDropped = false;
        logMessage("[MQTT] Dropped oversized, fragmented or busy command");
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (mqttConnecting || mqttClient.connected()) {
            mqttClient.disconnect(true);
            mqttConnecting = false;
        }
        return;
    }
    if (mqttClient.connected()) return;

    unsigned long now = millis();
    if (mqttConnecting) {
        if (now - mqttConnectStarted >= MQTT_CONNECT_TIMEOUT) {
            mqttClient.disconnect(true);
            mqttConnecting = false;
            scheduleMqttRetry();
            logMessage("[MQTT] Connect timeout (DNS/TCP/CONNACK), retry_ms=" + String(mqttReconnectInterval));
        }
        return;
    }
    if (now - lastMqttReconnectAttempt < mqttReconnectInterval) return;
    lastMqttReconnectAttempt = now;
    mqttConnectStarted = now;
    mqttConnecting = true;
    mqttRetryScheduled = false;
    logMessage("[MQTT] Connecting asynchronously");
    mqttClient.connect();
}

/** Queue a publish only while connected; never wait for the broker. */
void mqttPublish(const char* topic, const char* payload, bool retained) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, 0, retained, payload);
    }
}

/** Publish a retained value under the device's topic tree. */
void mqttPublishState(const char* key, const char* value) {
    String topic = String(MQTT_BASE_TOPIC) + "/" + key;
    mqttPublish(topic.c_str(), value, true);
}

/** Publish a non-retained device event. */
void mqttPublishEvent(const char* event) {
    mqttPublish(MQTT_EVENT_TOPIC, event, false);
}

/** Queue the extraction summary without waiting for network traffic. */
void mqttPublishEventJson(const char* event, float weight, float timeSec, const char* doseType, const char* stopReason,
                          float avgFlow, float peakFlow, float firstDrip) {
    if (!mqttClient.connected()) return;
    String json = "{\"event\":\"" + String(event) +
                  "\",\"weight\":" + String(weight, 1) +
                  ",\"time\":" + String(timeSec, 1) +
                  ",\"dose\":\"" + String(doseType) +
                  "\",\"reason\":\"" + String(stopReason) +
                  "\",\"avgFlow\":" + String(avgFlow, 2) +
                  ",\"peakFlow\":" + String(peakFlow, 2) +
                  ",\"firstDrip\":" + String(firstDrip, 1) + "}";
    mqttPublish(MQTT_EVENT_TOPIC, json.c_str(), false);
}
