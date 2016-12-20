#ifndef Button_h
#define Button_h

#define BUTTON_PULLUP_HIGH 1
#define BUTTON_PULLUP_LOW 0

#define NO_PRESS 0
#define SHORT_PRESS 1
#define LONG_PRESS 2


class Button {
  public:
    Button(unsigned char pin, unsigned char pullup);
    unsigned char getState();
    unsigned char processState();
  private:
    unsigned char _pin;
    unsigned char _pullup;    // 1 if pullup to +5, otherwise - 0
    unsigned int _shortPressDelay;
    unsigned int _longPressDelay;
    
    unsigned long  _pressTimer;
    unsigned char _previousState;
    unsigned char _buttonState; //0 - released; 1 - short press; 2 - in long press; 
                      //3 - was in short press and now waits for long or release
};

#endif
