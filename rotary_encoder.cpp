#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "rotary_encoder.h"

volatile byte rotaryEncodarAFlag    = 0;
volatile byte rotaryEncodarBFlag    = 0;
long          rotaryEncoderPosition = 0;
uint8_t       rotaryEncoderPinA     = 0;
uint8_t       rotaryEncoderPinB     = 0;

IRAM_ATTR
void handlePinAChange(){
  noInterrupts();
  byte pinA = digitalRead(rotaryEncoderPinA);
  byte pinB = digitalRead(rotaryEncoderPinB);
  if(pinA == HIGH && pinB == HIGH && rotaryEncodarAFlag) {
    rotaryEncoderPosition -= 1;
    rotaryEncodarBFlag = 0;
    rotaryEncodarAFlag = 0;
  }
  else if (pinA == HIGH) rotaryEncodarBFlag = 1;
  interrupts();
}

IRAM_ATTR
void handlePinBChange(){
  noInterrupts();
  byte pinA = digitalRead(rotaryEncoderPinA);
  byte pinB = digitalRead(rotaryEncoderPinB);
  if (pinA == HIGH && pinB == HIGH && rotaryEncodarBFlag) {
    rotaryEncoderPosition += 1;
    rotaryEncodarBFlag = 0;
    rotaryEncodarAFlag = 0;
  }
  else if (pinB == HIGH) rotaryEncodarAFlag = 1;
  interrupts();
}

void initRotaryEncoder(uint8_t pinA, uint8_t pinB) {
  rotaryEncoderPinA = pinA;
  rotaryEncoderPinB = pinB;

  pinMode(rotaryEncoderPinA, INPUT_PULLUP);
  pinMode(rotaryEncoderPinB, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(rotaryEncoderPinA), handlePinAChange, RISING);
  attachInterrupt(digitalPinToInterrupt(rotaryEncoderPinB), handlePinBChange, RISING);
}

long readRotaryEncoder() {
  return rotaryEncoderPosition;
}
