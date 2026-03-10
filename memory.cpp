#include <EEPROM.h>

#include "memory.h"

#define MAGIC_ADDRESS 32
#define MAGIC_VALUE   0xC0FFEE42UL

union UNI_LONG {
  unsigned long value;
  unsigned char bytes[4];
};

unsigned long readFromMemory (int address) {
  UNI_LONG result;
  for (int i = 0; i < 4; i++) {
    result.bytes[i] = EEPROM.read(address++);
  }
  return result.value;
}

void writeToMemory (int address, unsigned long value) {
  UNI_LONG result;
  result.value = value;
  for (int i = 0; i < 4; i++) {
    EEPROM.write(address++, result.bytes[i]);
  }
  EEPROM.commit();
}

void writeDefaults () {
  writeToMemory(0, 36000);   // singleDose: 36g
  writeToMemory(4, 50000);   // doubleDose: 50g
  writeToMemory(8, 0);       // shotCount
  writeToMemory(12, 0);      // lastShotTime
  writeToMemory(16, 2000);   // singleNaked overshoot: 2g
  writeToMemory(20, 2000);   // doubleNaked overshoot: 2g
  writeToMemory(24, 2000);   // singleSpouted overshoot: 2g
  writeToMemory(28, 2000);   // doubleSpouted overshoot: 2g
  writeToMemory(36, 1200);   // correctionTime: 1.2s
  writeToMemory(MAGIC_ADDRESS, MAGIC_VALUE);
}

bool isEepromBlank () {
  for (int i = 0; i < 32; i++) {
    if (EEPROM.read(i) != 0xFF) return false;
  }
  return true;
}

void initiateMemory () {
  EEPROM.begin(64);
  if (readFromMemory(MAGIC_ADDRESS) != MAGIC_VALUE) {
    if (isEepromBlank()) {
      // Truly uninitialized (first flash) — write sensible defaults
      writeDefaults();
    } else {
      // Existing settings from before the magic byte was introduced — keep them
      writeToMemory(MAGIC_ADDRESS, MAGIC_VALUE);
    }
  }
}
