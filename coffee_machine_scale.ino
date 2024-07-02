#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#include <credentials.h>
#include <JC_Button.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include "HX711.h"

#include "ota.h"
#include "memory.h"
#include "rotary_encoder.h"
#include "icons.h"

#define ENCODER_A       14   // Must be an interrupt pin for sufficient reliability
#define ENCODER_B       12   // Must be an interrupt pin for sufficient reliability
#define ENCODER_BUTTON  13
#define LONG_PRESS_MS   2000
#define BUS_CLOCK_SPEED 400000
#define RELAY_PIN       D8

#define SINGLE_DOSE_ADDRESS  0
#define DOUBLE_DOSE_ADDRESS  4
#define STATISTICS_ADDRESS   8
#define LAST_TIME_ADDRESS    12

#define OLED         U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define SMALL_FONT   u8g2_font_8x13B_tf // XE font group
#define LARGE_FONT   u8g2_font_fur30_tn // Free universal group

enum MenuItem {
  SingleDose,
  DoubleDose,
  Info,
  Scale
};

enum Screen {
  MenuScreen,
  SettingsScreen,
  Timer,
  InfoScreen,
  ScaleScreen
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
const char*    deviceId                 = "Coffee machine scale";

long           encoderPosition    = 0;
unsigned long  singleShotLimit    = 0;
unsigned long  doubleShotLimit    = 0;
unsigned long  grindedDosesCount  = 0;
unsigned long  lastShotTime       = 0;
unsigned long  lastActivityMillis = 0;
Screen         currentScreen      = MenuScreen;
MenuItem       menu[]             = {SingleDose, DoubleDose, Scale, Info};
MenuItem*      selectedItem       = menu;
short          menuLength         = sizeof(menu) / sizeof(*menu);
bool           ignoreNextPush     = false;

AsyncWebServer server(80);
OLED           display(U8G2_R0, U8X8_PIN_NONE);
Button         pushButton(ENCODER_BUTTON);
HX711          loadcellA;
HX711          loadcellB;

void drawMessage(const char *message) {
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    unsigned short stringWidth = display.getStrWidth(message);
    display.drawStr(64 - stringWidth / 2, 60, message);
    display.drawXBMP(64 - scaleIconBitmapWidth / 2, 0, scaleIconBitmapWidth, scaleIconBitmapHeight, scaleIconBitmap);
  } while (display.nextPage());
}

void showGrams(unsigned long milligrams, const char *title) {
  unsigned int grams = milligrams / 1000;
  byte gramDecimals = (milligrams % 1000) / 100;
  String gramsString = String(grams) + "." + String(gramDecimals);

  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, title);
    display.setFont(LARGE_FONT);
    unsigned short stringWidth = display.getStrWidth(grams < 10 ? "0:0" : "10:0");
    display.drawStr(64 - stringWidth / 2, 55, gramsString.c_str());
  } while (display.nextPage());
}

void showWeight(float loadCellWeight, const char *title) {
  if (abs(loadCellWeight) < 0.1) {
    loadCellWeight = 0;
  }

  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, title);
    display.setFont(LARGE_FONT);
    String weightString = String(loadCellWeight, 1);
    unsigned short stringWidth = display.getStrWidth(weightString.c_str());
    display.drawStr(64 - stringWidth / 2, 55, weightString.c_str());
  } while (display.nextPage());
}

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

void showInfo() {
  currentScreen = InfoScreen;
  String shotMadeString = String(grindedDosesCount);
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    display.drawStr(0, 15, (String("Shots: ") + String(grindedDosesCount)).c_str());
    display.drawStr(0, 30, (String("Last time: ") + String(float(lastShotTime) / 1000, 1)).c_str());
    display.drawStr(0, 45, (String("IP: ") + WiFi.localIP().toString()).c_str());
    display.drawStr(0, 60, (String("Net: ") + WiFi.SSID()).c_str());
  } while (display.nextPage());
}

float getTotalWeight() {
  float loadCellAWeight = loadcellA.get_units();
  float loadCellBWeight = loadcellB.get_units();
  float totalWeight = loadCellAWeight + loadCellBWeight;

  return totalWeight;
}

void showScale(const char *message) {
  currentScreen = ScaleScreen;

  const int weightArraySize = 5;
  float weightArray[weightArraySize] = { 0 };
  float driftCoefficient = 0.0f;

  while (true) {
    pushButton.read();
    if (pushButton.wasPressed()) {
      break;
    }

    float weightSum = 0.0f;
    bool canTare = true;
    for (int i = weightArraySize - 1; i >= 0; i--) {
      if (i > 0) {
        weightArray[i] = weightArray[i - 1];
      } else {
        weightArray[i] = getTotalWeight() + driftCoefficient;
      }
      weightSum += weightArray[i];
      if (abs(weightArray[i]) > 0.1) {
        canTare = false;
      }
    }

    float weightAverage = weightSum / weightArraySize;

    // We use this to prevent scale drifting
    if (abs(weightAverage) > 0.08 && abs(weightAverage) < 0.13 && canTare) {
      WebSerial.print(String("Calibrating to prevent drifting, array: {"));
      for (int i = 0; i < weightArraySize; i++) {
        WebSerial.print(String(weightArray[i]) + String(i == weightArraySize - 1 ? "} " : ", "));
      }
      WebSerial.println(String("Average: ") + String(weightAverage, 2));

      driftCoefficient -= weightAverage;
      weightAverage = 0;
    }

    showWeight(weightAverage, message);
    yield();
  };
}

void showMenu() {
  currentScreen = MenuScreen;
  display.firstPage();
  do {
    display.setFont(SMALL_FONT);
    for (short i = 0; i < menuLength; i++) {
      String selectionIndicator = *selectedItem == menu[i] ? "> " : "  ";
      String lineText = selectionIndicator + getMenuItemString(menu[i]);
      display.drawStr(0, 15 * (i + 1), lineText.c_str());
    }
  } while (display.nextPage());
}

void turnOnWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected. IP address:");
  Serial.println(WiFi.localIP());
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(D3, OUTPUT);
  digitalWrite(D3, LOW);
  pinMode(D4, OUTPUT);
  digitalWrite(D4, LOW);
  Serial.begin(115200);

  // Initiate rotary encoder
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  initRotaryEncoder(ENCODER_A, ENCODER_B);

  // Initiate load cells
  loadcellA.begin(LOADCELL_1_DOUT_PIN, LOADCELL_1_SCK_PIN);
  loadcellA.set_scale(LOADCELL_1_DIVIDER);
  loadcellA.set_offset(LOADCELL_1_OFFSET);

  loadcellB.begin(LOADCELL_2_DOUT_PIN, LOADCELL_2_SCK_PIN);
  loadcellB.set_scale(LOADCELL_2_DIVIDER);
  loadcellB.set_offset(LOADCELL_2_OFFSET);

  // Initiate the display
  display.begin();
  display.setBusClock(BUS_CLOCK_SPEED);

  display.firstPage();
  do {
    display.drawXBMP(
      64 - coffeeIconBitmapWidth / 2,
      32 - coffeeIconBitmapHeight / 2,
      coffeeIconBitmapWidth,
      coffeeIconBitmapHeight,
      coffeeIconBitmap
    );
  } while (display.nextPage());

  // Start WiFi
  turnOnWiFi();

  // Inititate eeprom memory
  initiateMemory();
  // writeToMemory(SINGLE_DOSE_ADDRESS, 0);
  // writeToMemory(DOUBLE_DOSE_ADDRESS, 0);
  // writeToMemory(STATISTICS_ADDRESS, 0);
  // writeToMemory(LAST_TIME_ADDRESS, 0);
  singleShotLimit = readFromMemory(SINGLE_DOSE_ADDRESS);
  doubleShotLimit = readFromMemory(DOUBLE_DOSE_ADDRESS);
  grindedDosesCount = readFromMemory(STATISTICS_ADDRESS);
  lastShotTime = readFromMemory(LAST_TIME_ADDRESS);

  // Initiate over the air programming
  OTA::initialize(deviceId);

  // Initiate WebSerial. It is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  WebSerial.msgCallback(handleWebSerialMessage);

  // Set up the html server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "The coffee machine scale is ready");
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Handle any other body request here...
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "The coffee machine scale says: sorry, not found.");
  });

  // Start the server
  server.begin();
  showMenu();
}

void handleWebSerialMessage(uint8_t *data, size_t len) {
  // Process message into command:value pair
  String command = "";
  String value = "";
  boolean beforeColon = true;
  for (int i = 0; i < len; i++) {
    if (char(data[i]) == ':') {
      beforeColon = false;
    } else if (beforeColon) {
      command += char(data[i]);
    } else {
      value += char(data[i]);
    }
  }

  if (command.equals("help")) {
    WebSerial.println((
                        String("Available commands:\n") + "getInfo\n"
                                                          "help\n")
                        .c_str());
  } else if (command.equals("getInfo")) {
    WebSerial.println(String("Total weight: ") + String("N/A"));
  } else {
    WebSerial.println(String("Unknown command '") + command + "' with value '" + value + "'" + value);
  }
}

void changeTargetWeight(unsigned long *targetWeightPtr, bool up, const char *title) {
  if (up) {
    *targetWeightPtr += 100;
  } else if (*targetWeightPtr >= 100) {
    *targetWeightPtr -= 100;
  }
  showGrams(*targetWeightPtr, title);
}

void handleRotation(bool clockwise) {
  switch (currentScreen) {
    case MenuScreen:
      if (clockwise) {
        if (*selectedItem == menu[menuLength - 1]) {
          selectedItem = menu;
        } else {
          selectedItem++;
        }
      } else {
        if (*selectedItem == menu[0]) {
          selectedItem = &menu[menuLength - 1];
        } else {
          selectedItem--;
        }
      }
      showMenu();
      break;
    case SettingsScreen:
      changeTargetWeight(
        (*selectedItem == SingleDose) ? &singleShotLimit : &doubleShotLimit,
        clockwise,
        getMenuItemString(*selectedItem));
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
          startExtraction(*selectedItem);
          break;
        }
        if (*selectedItem == Info) {
          showInfo();
          break;
        }
        if (*selectedItem == Scale) {
          drawMessage("Taring...");
          loadcellA.tare();
          loadcellB.tare();
          showScale("Weight (g):");
          break;
        }
      }
    case InfoScreen:
      {
        showMenu();
        break;
      }
    case SettingsScreen:
      {
        bool singleDose = *selectedItem == SingleDose;
        writeToMemory(
          singleDose ? SINGLE_DOSE_ADDRESS : DOUBLE_DOSE_ADDRESS,
          singleDose ? singleShotLimit : doubleShotLimit);
        showMenu();
        break;
      }
    case ScaleScreen:
      {
        showMenu();
        break;
      }
  }
}

void handleLongButtonPush() {
  switch (currentScreen) {
    case MenuScreen:
      if (*selectedItem == SingleDose || *selectedItem == DoubleDose) {
        currentScreen = SettingsScreen;
        showGrams(*selectedItem == SingleDose ? singleShotLimit : doubleShotLimit, getMenuItemString(*selectedItem));
        break;
      }
      if (*selectedItem == Info) {
        // Reset?
        break;
      }
    default:
      return;
  }
  ignoreNextPush = true;
}

void loop() {
  OTA::handle();

  // Process change in the rotary encoder position
  long newPosition = readRotaryEncoder();
  if (newPosition != encoderPosition) {
    handleRotation(newPosition - encoderPosition < 0);
    encoderPosition = newPosition;
    lastActivityMillis = millis();
  }

  // Process change in the button state
  pushButton.read();
  if (pushButton.wasReleased()) {
    handleButtonPush();
    lastActivityMillis = millis();
  } else if (pushButton.pressedFor(LONG_PRESS_MS)) {
    handleLongButtonPush();
    lastActivityMillis = millis();
  }

  //    if (millis() - lastActivityMillis > 30000) {
  //      sleep();
  //    }
}

void startExtraction(MenuItem selectedItem) {
  currentScreen = Timer;

  float finalTime = 0.0f;
  float timeLimit = 60.0f;  // Pump shouldn't run more than 60 senconds without rest
  bool stopped = false;
  float totalWeight = 0.0f;
  float weightLimit = selectedItem == SingleDose ? singleShotLimit / 1000 : doubleShotLimit / 1000;
  const int weightArraySize = 5;  // Size of the array
  float weightArray[weightArraySize] = { 0 };

  drawMessage("Taring...");
  loadcellA.tare();
  loadcellB.tare();

  unsigned long startTime = millis();
  digitalWrite(RELAY_PIN, HIGH);

  while (true) {
    pushButton.read();
    if (pushButton.wasReleased()) {
      stopped = true;
      break;
    }

    finalTime = (float)(millis() - startTime) / 1000;
    if (finalTime > timeLimit) {
      stopped = true;
      break;
    }

    totalWeight = getTotalWeight();
    float weightSum = 0.0f;
    for (int i = weightArraySize - 1; i >= 0; i--) {
      if (i > 0) {
        weightArray[i] = weightArray[i - 1];
      } else {
        weightArray[i] = totalWeight;
      }
      weightSum += weightArray[i];
    }
    float weightAverage = weightSum / weightArraySize;

    if (weightAverage > weightLimit) {
      break;
    }

    showWeight(weightAverage, (String("Pouring: ") + String(finalTime, 1) + "s").c_str());
    yield();
  }

  digitalWrite(RELAY_PIN, LOW);
  lastShotTime = int(finalTime * 1000);
  writeToMemory(LAST_TIME_ADDRESS, lastShotTime);

  if (stopped) {
    showScale((String("Stopped at ") + String(finalTime, 1) + "s").c_str());
  } else {
    writeToMemory(
      STATISTICS_ADDRESS,
      grindedDosesCount += (selectedItem == SingleDose ? 1 : 2));
    showScale((String("Done at ") + String(finalTime, 1) + "s").c_str());
  }
}
