#include <EEPROM.h>

#include "memory.h"

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

void initiateMemory () {
  EEPROM.begin(512);
}