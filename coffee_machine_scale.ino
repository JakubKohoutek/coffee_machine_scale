#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <credentials.h>
#include <JC_Button.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <LittleFS.h>
#include "HX711.h"

#include "ota.h"
#include "memory.h"
#include "rotary_encoder.h"
#include "icons.h"
#include "log.h"
#include "mqtt.h"
#include "web.h"

#define ENCODER_A       14   // Must be an interrupt pin for sufficient reliability
#define ENCODER_B       12   // Must be an interrupt pin for sufficient reliability
#define ENCODER_BUTTON  13
#define LONG_PRESS_MS   2000
#define BUS_CLOCK_SPEED 400000
#define RELAY_PIN       D8

#define MY_TZ           "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define MY_NTP_SERVER   "pool.ntp.org"
#define WIFI_CHECK_INTERVAL   30000
#define RSSI_LOG_INTERVAL     600000
#define MQTT_STATE_INTERVAL   30000

#define SINGLE_DOSE_ADDRESS  0
#define DOUBLE_DOSE_ADDRESS  4
#define STATISTICS_ADDRESS   8
#define LAST_TIME_ADDRESS    12
#define SINGLE_NAKED_ADDRESS 16
#define DOUBLE_NAKED_ADDRESS 20
#define SINGLE_SPOUT_ADDRESS 24
#define DOUBLE_SPOUT_ADDRESS 28
// MAGIC_ADDRESS is 32 (defined in memory.cpp)
#define CORRECTION_TIME_ADDRESS 36

#define OLED         U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define SMALL_FONT   u8g2_font_8x13B_tf // XE font group
#define LARGE_FONT   u8g2_font_fur30_tn // Free universal group

enum MenuItem {
  SingleDose,
  DoubleDose,
  Info,
  Scale
};

enum Portafilter {
  BottomLess,
  Spouted,
  Back
};

enum Screen {
  MenuScreen,
  SettingsScreen,
  Timer,
  InfoScreen,
  ScaleScreen,
  PortafilterScreen,
  OvershootScreen
};

// HX711 pins definition
const int LOADCELL_1_DOUT_PIN = D3;
const int LOADCELL_1_SCK_PIN = D4;
const int LOADCELL_2_DOUT_PIN = TX;
const int LOADCELL_2_SCK_PIN = RX;

// Loadcell callibration settings
const long LOADCELL_1_OFFSET = 177305;
const long LOADCELL_1_DIVIDER = 1799;
const long LOADCELL_2_OFFSET = 45415;
const long LOADCELL_2_DIVIDER = 1654;

const char*    ssid                     = STASSID;
const char*    password                 = STAPSK;
const char*    deviceId                 = "coffee-machine-scale";

long           encoderPosition       = 0;
unsigned long  singleShotLimit       = 0;
unsigned long  doubleShotLimit       = 0;
unsigned long  singleSpoutedOvershoot    = 0;
unsigned long  singleNakedOvershoot      = 0;
unsigned long  doubleSpoutedOvershoot    = 0;
unsigned long  doubleNakedOvershoot      = 0;
unsigned long  correctionTimeMs          = 1200;  // default 1.2s, stored in EEPROM as milliseconds
unsigned long  grindedDosesCount     = 0;
unsigned long  lastShotTime          = 0;
unsigned long  lastActivityMillis    = 0;
bool           sleepModeActive       = false;
Screen         currentScreen         = MenuScreen;
MenuItem       mainMenu[]                = { Scale, DoubleDose, SingleDose, Info };
MenuItem*      selectedItem          = mainMenu;
Portafilter    portafilterMenu[]     = { Spouted, BottomLess, Back };
Portafilter    selectedPortafilter   = Spouted;
short          menuLength            = sizeof(mainMenu) / sizeof(*mainMenu);
short          portafilterMenuLength = sizeof(portafilterMenu) / sizeof(*portafilterMenu);
bool           ignoreNextPush        = false;
unsigned long  lastWiFiCheck         = 0;
unsigned long  lastRssiLog           = 0;
unsigned long  lastMqttStatePublish  = 0;
bool           wifiConnected        = false;
WiFiEventHandler wifiDisconnectHandler;
bool           wifiDisconnectEvent  = false;
uint8_t        lastWiFiDisconnectReason = 0;
unsigned long  wifiDisconnectedAt   = 0;
unsigned long  wifiReconnectCount   = 0;
bool           otaStarted           = false;
unsigned long  slowOperationCount   = 0;
unsigned long  maxOperationMs       = 0;

// Shot profile storage (item 12)
#define MAX_PROFILE_POINTS 300
struct ProfilePoint {
  unsigned long timeMs;
  float weight;
};
ProfilePoint   shotProfile[MAX_PROFILE_POINTS];
int            profilePointCount    = 0;

AsyncWebServer server(80);
OLED           display(U8G2_R0, U8X8_PIN_NONE);
Button         pushButton(ENCODER_BUTTON);
HX711          loadcellA;
HX711          loadcellB;

const char *getMenuItemString(MenuItem menuItem) {
  switch (menuItem) {
    case SingleDose:
      return "Single shot";
    case DoubleDose:
      return "Double shot";
    case Scale:
      return "Scale";
    case Info:
      return "Info";
  }
}

unsigned long *getOvershootPtr() {
  if (*selectedItem == SingleDose) {
    return selectedPortafilter == BottomLess ? &singleNakedOvershoot : &singleSpoutedOvershoot;
  }
  // else it is double dose
  return selectedPortafilter == BottomLess ? &doubleNakedOvershoot : &doubleSpoutedOvershoot;
}

float getCurrentWeight() {
  float loadCellAWeight = loadcellA.get_units();
  float loadCellBWeight = loadcellB.get_units();
  float currentWeight = loadCellAWeight + loadCellBWeight;

  return currentWeight;
}

void showScale(const char *message) {
  currentScreen = ScaleScreen;

  const int weightArraySize = 5;
  float weightArray[weightArraySize] = { 0 };
  float driftCoefficient = 0.0f;

  while (true) {
    pushButton.read();
    if (pushButton.wasPressed()) {
      lastActivityMillis = millis();
      break;
    }

    float weightSum = 0.0f;
    bool canTare = true;
    for (int i = weightArraySize - 1; i >= 0; i--) {
      if (i > 0) {
        weightArray[i] = weightArray[i - 1];
      } else {
        weightArray[i] = getCurrentWeight() + driftCoefficient;
      }
      weightSum += weightArray[i];
      if (abs(weightArray[i]) > 0.1) {
        canTare = false;
      }
    }

    float weightAverage = weightSum / weightArraySize;

    // We use this to prevent scale drifting
    if (abs(weightAverage) > 0.08 && abs(weightAverage) < 0.13 && canTare) {
      String driftLog = "Calibrating drift, array: {";
      for (int i = 0; i < weightArraySize; i++) {
        driftLog += String(weightArray[i]) + String(i == weightArraySize - 1 ? "} " : ", ");
      }
      driftLog += "Average: " + String(weightAverage, 2);
      logMessage(driftLog);

      driftCoefficient -= weightAverage;
      weightAverage = 0;
    }

    showWeight(weightAverage, message);
    yield();
  };
}

/*
  Display functions - responsible for rendering views on the screen
*/

void showGramsWithTitle(unsigned long milligrams, const char *title) {
  unsigned int grams = milligrams / 1000;
  byte gramDecimals = (milligrams % 1000) / 100;
  String gramsString = String(grams) + "." + String(gramDecimals);

  display.clearBuffer();
  display.setFont(SMALL_FONT);
  display.drawStr(0, 15, title);
  display.setFont(LARGE_FONT);
  unsigned short stringWidth = display.getStrWidth(grams < 10 ? "0:0" : "10:0");
  display.drawStr(64 - stringWidth / 2, 55, gramsString.c_str());
  display.sendBuffer();
}

void showWelcomeScreen() {
  display.clearBuffer();
  display.drawXBMP(
    64 - coffeeIconBitmapWidth / 2,
    32 - coffeeIconBitmapHeight / 2,
    coffeeIconBitmapWidth,
    coffeeIconBitmapHeight,
    coffeeIconBitmap
  );
  display.sendBuffer();
}

void showTaringScreen() {
  const char * message = "Taring...";
  display.clearBuffer();
  display.setFont(SMALL_FONT);
  unsigned short stringWidth = display.getStrWidth(message);
  display.drawStr(64 - stringWidth / 2, 60, message);
  display.drawXBMP(64 - scaleIconBitmapWidth / 2, 0, scaleIconBitmapWidth, scaleIconBitmapHeight, scaleIconBitmap);
  display.sendBuffer();
}

/** Draw a WiFi icon, crossed out until the station has an IP address. */
void showWiFiIcon() {
  const uint8_t x = 114;
  const uint8_t y = 2;
  display.drawXBMP(x, y, wifiIconBitmapWidth, wifiIconBitmapHeight, wifiIconBitmap);
  if (WiFi.status() != WL_CONNECTED) {
    display.setDrawColor(0);
    display.drawLine(x, y - 1, x + 12, y + 9);
    display.drawLine(x, y + 1, x + 12, y + 11);
    display.setDrawColor(1);
    display.drawLine(x, y, x + 12, y + 10);
  }
}

/** Render menu choices with network status in the unused top-right corner. */
void showMainMenu() {
  currentScreen = MenuScreen;

  display.clearBuffer();
  display.setFont(SMALL_FONT);
  for (short i = 0; i < menuLength; i++) {
    String selectionIndicator = *selectedItem == mainMenu[i] ? "> " : "  ";
    String lineText = selectionIndicator + getMenuItemString(mainMenu[i]);
    display.drawStr(0, 15 * (i + 1), lineText.c_str());
  }
  showWiFiIcon();
  display.sendBuffer();
}

void showPortafilterMenu() {
  currentScreen = PortafilterScreen;

  display.clearBuffer();
  display.setFont(SMALL_FONT);
  display.drawStr(0, 15, "Portafilter:");
  for (short i = 0; i < portafilterMenuLength; i++) {
    String selectionIndicator = selectedPortafilter == portafilterMenu[i] ? "> " : "  ";
    String lineText = selectionIndicator + (portafilterMenu[i] == BottomLess ? "Bottomless" : portafilterMenu[i] == Spouted ? "Spouted" : "Back");
    display.drawStr(0, 15 * (i + 2), lineText.c_str());
  }
  display.sendBuffer();
}

void showWeight(float loadCellWeight, const char *title) {
  if (abs(loadCellWeight) < 0.1) {
    loadCellWeight = 0;
  }

  display.clearBuffer();
  display.setFont(SMALL_FONT);
  display.drawStr(0, 15, title);
  display.setFont(LARGE_FONT);
  String weightString = String(loadCellWeight, 1);
  unsigned short stringWidth = display.getStrWidth(weightString.c_str());
  display.drawStr(64 - stringWidth / 2, 55, weightString.c_str());
  display.sendBuffer();
}

void showExtractionWeight(float weight, float timeSec, float targetWeight) {
  if (abs(weight) < 0.1) weight = 0;

  display.clearBuffer();

  // Title: "12.3s / 36.0g"
  display.setFont(SMALL_FONT);
  String title = String(timeSec, 1) + "s / " + String(targetWeight, 1) + "g";
  unsigned short titleWidth = display.getStrWidth(title.c_str());
  display.drawStr(64 - titleWidth / 2, 13, title.c_str());

  // Current weight, large
  display.setFont(LARGE_FONT);
  String weightString = String(weight, 1);
  unsigned short stringWidth = display.getStrWidth(weightString.c_str());
  display.drawStr(64 - stringWidth / 2, 50, weightString.c_str());

  // Progress bar at bottom (4px tall)
  float progress = targetWeight > 0 ? weight / targetWeight : 0;
  if (progress > 1.0f) progress = 1.0f;
  if (progress < 0) progress = 0;
  int barWidth = (int)(progress * 128);
  display.drawBox(0, 60, barWidth, 4);

  display.sendBuffer();
}

void saveProfile() {
  File f = LittleFS.open("/last_shot.json", "w");
  if (!f) return;
  f.print("[");
  for (int i = 0; i < profilePointCount; i++) {
    if (i > 0) f.print(",");
    f.print("{\"t\":");
    f.print(shotProfile[i].timeMs);
    f.print(",\"w\":");
    f.print(String(shotProfile[i].weight, 1));
    f.print("}");
  }
  f.print("]");
  f.close();
}

/** Show device information with a compact IP row that fits any IPv4 address. */
void showInfo() {
  currentScreen = InfoScreen;

  display.clearBuffer();
  display.setFont(SMALL_FONT);
  display.drawStr(0, 15, (String("Shots: ") + String(grindedDosesCount)).c_str());
  display.drawStr(0, 30, (String("Last time: ") + String(float(lastShotTime) / 1000, 1)).c_str());
  display.setFont(u8g2_font_6x10_tf);
  String ipLine = WiFi.status() == WL_CONNECTED
    ? String("IP: ") + WiFi.localIP().toString() : String("IP: offline");
  display.drawStr(0, 45, ipLine.c_str());
  display.setFont(SMALL_FONT);
  display.drawStr(0, 60, (String("Net: ") + WiFi.SSID()).c_str());
  display.sendBuffer();
}

/*
  Helper functions
*/

/** Start association in the background so the menu is usable without a network. */
void turnOnWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(deviceId);
  WiFi.setAutoReconnect(true);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    lastWiFiDisconnectReason = event.reason;
    wifiDisconnectEvent = true;
  });
  wifiDisconnectedAt = lastWiFiCheck = millis();
  WiFi.begin(ssid, password);
  logMessage("[WIFI] Connecting in background; local controls available offline");
}

/** Observe link changes and request retries without waiting for association or DHCP. */
void handleWiFi() {
  unsigned long now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (wifiDisconnectEvent) {
    wifiDisconnectEvent = false;
    logMessage("[WIFI] Disconnected, reason=" + String(lastWiFiDisconnectReason) +
               ", status=" + String(WiFi.status()));
  }
  if (connected != wifiConnected) {
    wifiConnected = connected;
    if (currentScreen == MenuScreen) showMainMenu();
    else if (currentScreen == InfoScreen) showInfo();
    if (connected) {
      logMessage("[WIFI] Connected. IP: " + WiFi.localIP().toString() +
                 ", offline_ms=" + String(now - wifiDisconnectedAt) +
                 ", RSSI=" + String(WiFi.RSSI()));
    } else {
      wifiDisconnectedAt = lastWiFiCheck = now;
      logMessage("[WIFI] Link unavailable; controls remain active");
    }
  }

  if (!connected) {
    if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
      lastWiFiCheck = now;
      ++wifiReconnectCount;
      bool requested = WiFi.reconnect();
      logMessage("[WIFI] Background retry=" + String(wifiReconnectCount) +
                 ", requested=" + String(requested) + ", status=" + String(WiFi.status()));
    }
    return;
  }

  if (now - lastRssiLog >= RSSI_LOG_INTERVAL) {
    lastRssiLog = now;
    int rssi = WiFi.RSSI();
    const char* quality = rssi >= -50 ? "Excellent" : rssi >= -60 ? "Good" : rssi >= -70 ? "Fair" : rssi >= -80 ? "Weak" : "Poor";
    logMessage(String("[WIFI] RSSI: ") + rssi + " dBm (" + quality + ")");
  }
}

/** Report unexpectedly slow work, rate-limited to avoid a diagnostic log storm. */
void logSlowOperation(const char* operation, unsigned long started) {
  unsigned long elapsed = millis() - started;
  if (elapsed < 250) return;
  ++slowOperationCount;
  maxOperationMs = max(maxOperationMs, elapsed);
  static unsigned long lastReport = 0;
  if (slowOperationCount != 1 && millis() - lastReport < 30000) return;
  lastReport = millis();
  logMessage(String("[SLOW] ") + operation + ", elapsed_ms=" + String(elapsed) +
             ", screen=" + String(currentScreen) + ", heap=" + String(ESP.getFreeHeap()));
}

/** Service optional networking and diagnostics outside the pump-control loop. */
void handleBackground() {
  unsigned long started = millis();
  handleWiFi();
  logSlowOperation("wifi", started);

  started = millis();
  if (wifiConnected && !otaStarted) {
    OTA::initialize(deviceId);
    otaStarted = true;
    logMessage("[OTA] Ready");
  }
  if (otaStarted) OTA::handle();
  logSlowOperation("ota", started);

  started = millis();
  handleMqtt();
  unsigned long now = millis();
  if (now - lastMqttStatePublish >= MQTT_STATE_INTERVAL) {
    lastMqttStatePublish = now;
    publishAllStates();
  }
  logSlowOperation("mqtt", started);

  static unsigned long lastHealthLog = 0;
  if (now - lastHealthLog >= 60000) {
    lastHealthLog = now;
    logMessage("[HEALTH] heap=" + String(ESP.getFreeHeap()) +
               ", max_block=" + String(ESP.getMaxFreeBlockSize()) +
               ", fragmentation=" + String(ESP.getHeapFragmentation()) +
               ", wifi=" + String(WiFi.status()) + ", mqtt=" + String(mqttClient.connected()) +
               ", screen=" + String(currentScreen) + ", slow_count=" + String(slowOperationCount) +
               ", slow_max_ms=" + String(maxOperationMs) + ", log_max_ms=" + String(maxLogWriteMs));
  }
}

void handleCommand(const String& cmd) {
  String command = "";
  String value = "";
  int colonIndex = cmd.indexOf(':');
  if (colonIndex > 0) {
    command = cmd.substring(0, colonIndex);
    value = cmd.substring(colonIndex + 1);
  } else {
    command = cmd;
  }

  unsigned long val = value.toInt();

  // Validate: reject non-numeric input and unreasonable values (>200g)
  if (colonIndex > 0) {
    if (val == 0 && value != "0") {
      logMessage("[CMD] Invalid value (non-numeric): " + value);
      return;
    }
    if (val > 200000) {
      logMessage("[CMD] Value too large (>200g): " + value);
      return;
    }
  }

  if (command == "setSingleDose") {
    singleShotLimit = val;
    writeToMemory(SINGLE_DOSE_ADDRESS, singleShotLimit);
    logMessage("[CMD] singleShotLimit = " + String(singleShotLimit));
  } else if (command == "setDoubleDose") {
    doubleShotLimit = val;
    writeToMemory(DOUBLE_DOSE_ADDRESS, doubleShotLimit);
    logMessage("[CMD] doubleShotLimit = " + String(doubleShotLimit));
  } else if (command == "setSingleNaked") {
    singleNakedOvershoot = val;
    writeToMemory(SINGLE_NAKED_ADDRESS, singleNakedOvershoot);
    logMessage("[CMD] singleNakedOvershoot = " + String(singleNakedOvershoot));
  } else if (command == "setSingleSpouted") {
    singleSpoutedOvershoot = val;
    writeToMemory(SINGLE_SPOUT_ADDRESS, singleSpoutedOvershoot);
    logMessage("[CMD] singleSpoutedOvershoot = " + String(singleSpoutedOvershoot));
  } else if (command == "setDoubleNaked") {
    doubleNakedOvershoot = val;
    writeToMemory(DOUBLE_NAKED_ADDRESS, doubleNakedOvershoot);
    logMessage("[CMD] doubleNakedOvershoot = " + String(doubleNakedOvershoot));
  } else if (command == "setDoubleSpouted") {
    doubleSpoutedOvershoot = val;
    writeToMemory(DOUBLE_SPOUT_ADDRESS, doubleSpoutedOvershoot);
    logMessage("[CMD] doubleSpoutedOvershoot = " + String(doubleSpoutedOvershoot));
  } else if (command == "setCorrectionTime") {
    correctionTimeMs = val;
    writeToMemory(CORRECTION_TIME_ADDRESS, correctionTimeMs);
    logMessage("[CMD] correctionTimeMs = " + String(correctionTimeMs));
  } else {
    logMessage("[CMD] Unknown command: " + cmd);
  }
}

void publishAllStates() {
  mqttPublishState("singleDose", String(singleShotLimit).c_str());
  mqttPublishState("doubleDose", String(doubleShotLimit).c_str());
  mqttPublishState("singleNakedOvershoot", String(singleNakedOvershoot).c_str());
  mqttPublishState("singleSpoutedOvershoot", String(singleSpoutedOvershoot).c_str());
  mqttPublishState("doubleNakedOvershoot", String(doubleNakedOvershoot).c_str());
  mqttPublishState("doubleSpoutedOvershoot", String(doubleSpoutedOvershoot).c_str());
  mqttPublishState("shotCount", String(grindedDosesCount).c_str());
  mqttPublishState("lastShotTime", String(lastShotTime).c_str());
  mqttPublishState("sleep", sleepModeActive ? "true" : "false");
  mqttPublish(MQTT_AVAILABLE_TOPIC, "online", true);
}

void changeTargetWeight(unsigned long *targetWeightPtr, bool up, const char *title) {
  if (up) {
    *targetWeightPtr += 100;
  } else if (*targetWeightPtr >= 100) {
    *targetWeightPtr -= 100;
  }
  showGramsWithTitle(*targetWeightPtr, title);
}

void changeOvershoot(unsigned long *weightCoeffPtr, bool up, const char *title) {
  if (up) {
    *weightCoeffPtr += 100;
  } else if (*weightCoeffPtr >= 100) {
    *weightCoeffPtr -= 100;
  }
  showGramsWithTitle(*weightCoeffPtr, title);
}

void writeOvershootToMemory() {
  unsigned long newWeightCoefficient = *getOvershootPtr();
  if (*selectedItem == SingleDose && selectedPortafilter == BottomLess) {
    writeToMemory(SINGLE_NAKED_ADDRESS, newWeightCoefficient);
  }

  if (*selectedItem == SingleDose && selectedPortafilter == Spouted) {
    writeToMemory(SINGLE_SPOUT_ADDRESS, newWeightCoefficient);
  }

  if (*selectedItem == DoubleDose && selectedPortafilter == BottomLess) {
    writeToMemory(DOUBLE_NAKED_ADDRESS, newWeightCoefficient);
  }

  if (*selectedItem == DoubleDose && selectedPortafilter == Spouted) {
    writeToMemory(DOUBLE_SPOUT_ADDRESS, newWeightCoefficient);
  }
}

void handleRotation(bool clockwise) {
  switch (currentScreen) {
    case MenuScreen:
      if (clockwise) {
        if (*selectedItem == mainMenu[menuLength - 1]) {
          selectedItem = mainMenu;
        } else {
          selectedItem++;
        }
      } else {
        if (*selectedItem == mainMenu[0]) {
          selectedItem = &mainMenu[menuLength - 1];
        } else {
          selectedItem--;
        }
      }
      showMainMenu();
      break;
    case SettingsScreen:
      changeTargetWeight(
        (*selectedItem == SingleDose) ? &singleShotLimit : &doubleShotLimit,
        clockwise,
        getMenuItemString(*selectedItem));
      break;
    case PortafilterScreen:
      {
        if (clockwise) {
          if (selectedPortafilter == Spouted)
            selectedPortafilter = BottomLess;
          else if (selectedPortafilter == BottomLess)
            selectedPortafilter = Back;
          else if (selectedPortafilter == Back)
            selectedPortafilter = Spouted;
        } else {
          if (selectedPortafilter == Spouted)
            selectedPortafilter = Back;
          else if (selectedPortafilter == Back)
            selectedPortafilter = BottomLess;
          else if (selectedPortafilter == BottomLess)
            selectedPortafilter = Spouted;
        }
        showPortafilterMenu();
        break;
      }
    case OvershootScreen:
      changeOvershoot(
        getOvershootPtr(),
        clockwise,
        selectedPortafilter == BottomLess ? "Bottomless" : "Spouted");
      break;
  }
}

void handleButtonPush() {
  if (ignoreNextPush) {
    ignoreNextPush = false;
    return;
  }

  switch (currentScreen) {
    case MenuScreen:
      {
        if (*selectedItem == SingleDose || *selectedItem == DoubleDose) {
          showPortafilterMenu();
          break;
        }
        if (*selectedItem == Info) {
          showInfo();
          break;
        }
        if (*selectedItem == Scale) {
          showTaringScreen();
          loadcellA.tare();
          loadcellB.tare();
          showScale("Weight (g):");
          break;
        }
        break;
      }
    case PortafilterScreen:
      {
        if (selectedPortafilter == Back) {
          showMainMenu();
          break;
        }
        startExtraction();
        break;
      }
    case InfoScreen:
      {
        showMainMenu();
        break;
      }
    case SettingsScreen:
      {
        bool singleDose = *selectedItem == SingleDose;
        writeToMemory(
          singleDose ? SINGLE_DOSE_ADDRESS : DOUBLE_DOSE_ADDRESS,
          singleDose ? singleShotLimit : doubleShotLimit);
        showMainMenu();
        break;
      }
    case OvershootScreen:
      {
        writeOvershootToMemory();
        showPortafilterMenu();
        break;
      }
    case ScaleScreen:
      {
        showMainMenu();
        break;
      }
  }
}

void handleLongButtonPush() {
  switch (currentScreen) {
    case MenuScreen:
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose) {
        currentScreen = SettingsScreen;
        showGramsWithTitle(*selectedItem == SingleDose ? singleShotLimit : doubleShotLimit, getMenuItemString(*selectedItem));
        break;
      }
      if (*selectedItem == Info) {
        // Reset?
        break;
      }
    case PortafilterScreen:
      {
        if (selectedPortafilter == Back) {
          return;
        }
        currentScreen = OvershootScreen;
        unsigned long * weightCoeffPtr = getOvershootPtr();
        showGramsWithTitle(*weightCoeffPtr, selectedPortafilter == BottomLess ? "Bottomless" : "Spouted");
        break;
      }
    default:
      return;
  }
  ignoreNextPush = true;
}

float medianOfFive(float a[], int n) {
  float tmp[5];
  for (int i = 0; i < n; i++) tmp[i] = a[i];
  // Simple insertion sort (n <= 5)
  for (int i = 1; i < n; i++) {
    float key = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = key;
  }
  return tmp[n / 2];
}

/** Run extraction and settling, then keep the result screen responsive until dismissed. */
void startExtraction() {
  currentScreen = Timer;

  float timePassed = 0.0f;
  float timeLimit = 60.0f;  // Pump shouldn't run more than 60 seconds without rest
  bool stopped = false;
  float currentWeight = 0.0f;
  float overshoot = *getOvershootPtr() / 1000.0f;
  float weightLimit = *selectedItem == SingleDose ? singleShotLimit / 1000.0f : doubleShotLimit / 1000.0f;
  const int weightArraySize = 5;
  float weightArray[weightArraySize] = { 0.0f };
  int numberOfReadings = 0;
  float finalWeight = 0.0f;
  const char* doseType = *selectedItem == SingleDose ? "single" : "double";

  // Reset shot profile
  profilePointCount = 0;
  unsigned long lastProfileCapture = 0;

  showTaringScreen();
  loadcellA.tare();
  loadcellB.tare();

  unsigned long startTime = millis();
  // due to the current the relay needs to switch on and vibrations in the machine there is a certain
  // level of fault in the measurement once the pump is turned on. This variable helps to correct that
  float correctionWeight = 0.0f;
  bool correctionApplied = false;
  float timeOfCorrection = correctionTimeMs / 1000.0f;

  logMessage("[EXTRACTION] Starting, target: " + String(weightLimit, 1) + "g, overshoot: " + String(overshoot, 1) + "g");
  mqttPublishEvent("extraction_started");

  digitalWrite(RELAY_PIN, HIGH);

  while (true) {
    ESP.wdtFeed();

    // Stop the extraction if the button is pushed
    pushButton.read();
    if (pushButton.wasReleased()) {
      stopped = true;
      break;
    }

    // Stop the extraction if we pass the safety time limit
    timePassed = (float)(millis() - startTime) / 1000;
    if (timePassed > timeLimit) {
      stopped = true;
      break;
    }

    // Get the current sensor reading
    currentWeight = getCurrentWeight();

    // Vibrations and voltage drop if the relay is switched on can cause incorrect weight readings
    // at the beginning of extraction. Ignore the readings before the tray, cups and other things settle in place.
    if (timePassed < timeOfCorrection) {
      // Capture zero-weight profile points so the chart shows the initial flat period
      unsigned long now = millis();
      if (now - lastProfileCapture >= 200 && profilePointCount < MAX_PROFILE_POINTS) {
        shotProfile[profilePointCount].timeMs = now - startTime;
        shotProfile[profilePointCount].weight = 0;
        profilePointCount++;
        lastProfileCapture = now;
      }
      showExtractionWeight(0, timePassed, weightLimit);
      yield();
      continue;
    }

    // zeroing the weight once, after timeOfCorrection passed.
    if (!correctionApplied) {
      correctionApplied = true;
      correctionWeight = currentWeight;
    }

    // Calculate the median of recent weight readings (rejects vibration spikes with less lag than averaging)
    numberOfReadings = numberOfReadings >= weightArraySize ? weightArraySize : numberOfReadings + 1;
    for (int i = numberOfReadings - 1; i >= 0; i--) {
      if (i > 0) {
        weightArray[i] = weightArray[i - 1];
      } else {
        weightArray[i] = currentWeight;
      }
    }

    float medianWeight = medianOfFive(weightArray, numberOfReadings);
    float correctedWeight = medianWeight - correctionWeight;
    // If the corrected weight is negative, zero it out to the current level
    if (correctedWeight < 0.0f) {
      correctionWeight = medianWeight;
      correctedWeight = 0;
    }

    finalWeight = correctedWeight;

    // Capture profile data every 200ms
    unsigned long now = millis();
    if (now - lastProfileCapture >= 200 && profilePointCount < MAX_PROFILE_POINTS) {
      shotProfile[profilePointCount].timeMs = now - startTime;
      shotProfile[profilePointCount].weight = correctedWeight;
      profilePointCount++;
      lastProfileCapture = now;
    }

    // Stop the extraction once we reach the target weight minus overshoot
    if (correctedWeight > weightLimit - overshoot) {
      break;
    }

    // Show the current weight with progress bar and target
    showExtractionWeight(correctionApplied ? correctedWeight : 0, timePassed, weightLimit);

    yield();
  }

  digitalWrite(RELAY_PIN, LOW);

  // Compute title and stats before settling so the display is consistent throughout
  const char* stopReason;
  if (stopped) {
    stopReason = "manual";
  } else {
    writeToMemory(
      STATISTICS_ADDRESS,
      grindedDosesCount += (*selectedItem == SingleDose ? 1 : 2));
    stopReason = "target_reached";
  }

  // Wait 3 seconds after pump stops for remaining liquid to flow into cup.
  // Show the result screen (same layout as final) so there's no visual jump.
  // Button press skips the wait.
  unsigned long settleStart = millis();
  while (millis() - settleStart < 3000) {
    ESP.wdtFeed();
    pushButton.read();
    if (pushButton.wasReleased()) {
      break;
    }

    currentWeight = getCurrentWeight();

    // Continue updating the median filter during settling
    for (int i = weightArraySize - 1; i >= 0; i--) {
      if (i > 0) {
        weightArray[i] = weightArray[i - 1];
      } else {
        weightArray[i] = currentWeight;
      }
    }
    float medianWeight = medianOfFive(weightArray, weightArraySize);
    float correctedWeight = medianWeight - correctionWeight;
    if (correctedWeight < 0.0f) {
      correctionWeight = medianWeight;
      correctedWeight = 0;
    }
    finalWeight = correctedWeight;

    // Continue capturing profile during settling
    unsigned long now = millis();
    if (now - lastProfileCapture >= 200 && profilePointCount < MAX_PROFILE_POINTS) {
      shotProfile[profilePointCount].timeMs = now - startTime;
      shotProfile[profilePointCount].weight = correctedWeight;
      profilePointCount++;
      lastProfileCapture = now;
    }

    timePassed = (float)(millis() - startTime) / 1000;
    String settleTitle = stopped
      ? String("Stopped at ") + String(timePassed, 1) + "s"
      : String("Done at ") + String(timePassed, 1) + "s";
    showWeight(finalWeight, settleTitle.c_str());
    yield();
  }

  timePassed = (float)(millis() - startTime) / 1000;
  lastShotTime = int(timePassed * 1000);
  writeToMemory(LAST_TIME_ADDRESS, lastShotTime);

  String title = stopped
    ? String("Stopped at ") + String(timePassed, 1) + "s"
    : String("Done at ") + String(timePassed, 1) + "s";

  logMessage(String("[EXTRACTION] ") + title + ", weight: " + String(finalWeight, 1) + "g");
  mqttPublishEventJson(stopped ? "extraction_stopped" : "extraction_complete",
                       finalWeight, timePassed, doseType, stopReason);

  // Save shot profile to LittleFS
  unsigned long saveStarted = millis();
  saveProfile();
  logSlowOperation("save-profile", saveStarted);

  // Show locked final weight until button release
  showWeight(finalWeight, title.c_str());
  while (true) {
    pushButton.read();
    if (pushButton.wasReleased()) break;
    handleBackground();
    yield();
  }
  lastActivityMillis = millis();
  encoderPosition = readRotaryEncoder();
  showMainMenu();
}

bool goToSleepMode () {
  if (sleepModeActive) {
    return false;
  }

  sleepModeActive = true;
  display.setPowerSave(1);
  return true;
}

bool wakeUp () {
  if (!sleepModeActive) {
    return false;
  }

  sleepModeActive = false;
  display.setPowerSave(0);
  return true;
}

/** Initialize local controls before starting optional, asynchronous network services. */
void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(D3, OUTPUT);
  digitalWrite(D3, LOW);
  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);
  Serial.begin(115200);

  // Enable hardware watchdog for extraction safety
  ESP.wdtEnable(8000);

  initiateLog();
  logMessage("[BOOT] Reset reason: " + ESP.getResetReason() + ", heap=" + String(ESP.getFreeHeap()));
  logMessage("[BOOT] Reset details: " + ESP.getResetInfo());

  // Initiate rotary encoder
  pushButton.begin();
  initRotaryEncoder(ENCODER_A, ENCODER_B);

  // Initiate load cells
  loadcellA.begin(LOADCELL_1_DOUT_PIN, LOADCELL_1_SCK_PIN);
  loadcellA.set_scale(LOADCELL_1_DIVIDER);
  loadcellA.set_offset(LOADCELL_1_OFFSET);

  loadcellB.begin(LOADCELL_2_DOUT_PIN, LOADCELL_2_SCK_PIN);
  loadcellB.set_scale(LOADCELL_2_DIVIDER);
  loadcellB.set_offset(LOADCELL_2_OFFSET);

  // Initiate the display and show welcome screen as soon as possible
  display.begin();
  display.setBusClock(BUS_CLOCK_SPEED);
  showWelcomeScreen();
  logMessage("[BOOT] Display, load cells and controls initialized");

  // Inititate eeprom and load variables from persistent memory
  initiateMemory();
  singleShotLimit    = readFromMemory(SINGLE_DOSE_ADDRESS);
  doubleShotLimit    = readFromMemory(DOUBLE_DOSE_ADDRESS);
  grindedDosesCount  = readFromMemory(STATISTICS_ADDRESS);
  lastShotTime       = readFromMemory(LAST_TIME_ADDRESS);
  singleSpoutedOvershoot = readFromMemory(SINGLE_SPOUT_ADDRESS);
  singleNakedOvershoot   = readFromMemory(SINGLE_NAKED_ADDRESS);
  doubleSpoutedOvershoot = readFromMemory(DOUBLE_SPOUT_ADDRESS);
  doubleNakedOvershoot   = readFromMemory(DOUBLE_NAKED_ADDRESS);
  correctionTimeMs       = readFromMemory(CORRECTION_TIME_ADDRESS);
  // Existing devices have uninitialized data at this address — use default if out of range
  if (correctionTimeMs == 0 || correctionTimeMs > 10000) {
    correctionTimeMs = 1200;
    writeToMemory(CORRECTION_TIME_ADDRESS, correctionTimeMs);
  }

  logMessage("[BOOT] Settings loaded");

  turnOnWiFi();
  configTime(MY_TZ, MY_NTP_SERVER);

  // Initiate MQTT
  initiateMqtt();

  // Web interface
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"singleDose\":" + String(singleShotLimit) + ",";
    json += "\"doubleDose\":" + String(doubleShotLimit) + ",";
    json += "\"singleNaked\":" + String(singleNakedOvershoot) + ",";
    json += "\"singleSpouted\":" + String(singleSpoutedOvershoot) + ",";
    json += "\"doubleNaked\":" + String(doubleNakedOvershoot) + ",";
    json += "\"doubleSpouted\":" + String(doubleSpoutedOvershoot) + ",";
    json += "\"shotCount\":" + String(grindedDosesCount) + ",";
    json += "\"lastShotTime\":" + String(lastShotTime) + ",";
    json += "\"sleep\":" + String(sleepModeActive ? "true" : "false") + ",";
    json += "\"correctionTime\":" + String(correctionTimeMs) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/send", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("cmd")) {
      String cmd = request->getParam("cmd")->value();
      handleCommand(cmd);
      request->send(200, "text/plain", "OK: " + cmd);
    } else {
      request->send(400, "text/plain", "Missing cmd parameter");
    }
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, logFilePath, "text/plain");
  });

  server.on("/log/previous", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists(previousLogFilePath)) {
      request->send(LittleFS, previousLogFilePath, "text/plain");
    } else {
      request->send(404, "text/plain", "No previous log");
    }
  });

  server.on("/api/profile", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/last_shot.json")) {
      request->send(LittleFS, "/last_shot.json", "application/json");
    } else {
      request->send(200, "application/json", "[]");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found. Try / for web UI.");
  });

  // Start the server
  server.begin();

  showMainMenu();

  lastActivityMillis = millis();
  logMessage("[BOOT] Menu ready, setup_ms=" + String(millis()) + ", wifi=" + String(WiFi.status()));
}

/** Prioritize physical input before servicing nonblocking network tasks. */
void loop() {
  // Process change in the rotary encoder position
  long newPosition = readRotaryEncoder();
  if (newPosition != encoderPosition) {
    lastActivityMillis = millis();
    // Ignore the rotation that woke up the device from sleep
    if (wakeUp()) {
      return;
    }
    handleRotation(newPosition - encoderPosition < 0);
    encoderPosition = newPosition;
  }

  // Process change in the button state
  pushButton.read();
  if (pushButton.wasReleased()) {
    lastActivityMillis = millis();
    // Ignore the press that woke up the device from sleep
    if (wakeUp()) {
      return;
    }
    handleButtonPush();
  } else if (pushButton.pressedFor(LONG_PRESS_MS)) {
    lastActivityMillis = millis();
    // Ignore the long press that woke up the device from sleep
    if (wakeUp()) {
      return;
    }
    handleLongButtonPush();
  }

  if (millis() - lastActivityMillis > 5 * 60 * 1000) {
    goToSleepMode();
  }
  handleBackground();
}
