#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

void handlePinAChange  ();
void handlePinBChange  ();
void initRotaryEncoder (uint8_t pinA, uint8_t pinB);
long readRotaryEncoder ();

#endif // ROTARY_ENCODER_H