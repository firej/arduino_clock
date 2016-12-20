#include "Button.h"
#include "Arduino.h"

Button::Button(unsigned char pin, unsigned char pullup){
    _pin = pin;
    _pullup = pullup;
    _shortPressDelay = 5;
    _longPressDelay = 400;
    _buttonState = 0;
    _previousState = 0;
    if (BUTTON_PULLUP_HIGH == pullup) {
        pinMode(pin, INPUT_PULLUP);
        // alternative - pinMode(pin,INPUT) && digitalWrite(pin, HIGH);
    }
    else {
        pinMode(pin, INPUT);
    }
}

unsigned char Button::getState(){
    int readout0 = digitalRead(_pin);
    delay(2);
    int readout1 = digitalRead(_pin);
    if (readout0 == readout1){
        return readout1;
    }
    else {
        return _pullup;     // return default button state
    }
}

unsigned char Button::processState(){
    unsigned char _state = getState();

    // button is not pressed
    if (_state == _pullup && _previousState == _pullup) {
        return NO_PRESS;
    }

    // button pressed and this is first check
    if (_state == !_pullup && _previousState == _pullup) {
        _previousState = !_pullup;
        _pressTimer = millis();
        return NO_PRESS;
    }

    // button is pressed
    if (_state == !_pullup && _previousState == !_pullup) {
        return NO_PRESS;
    }

    // button is released
    if (_state == _pullup && _previousState == !_pullup) {
      unsigned long currentTime = millis();
      _previousState = _pullup;
      if (currentTime > (_pressTimer + _longPressDelay)){
          return LONG_PRESS;
      }
      else if (currentTime > (_pressTimer + _shortPressDelay)){
          return SHORT_PRESS;
      }
    }
    return NO_PRESS;
}

