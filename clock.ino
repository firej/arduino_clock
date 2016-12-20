#include <FastLED.h>
#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <EEPROM.h>
#include "./Button.h"


#define NUM_CLOCKS          2
#define PIXELS_FOR_CLOCK    29
#define NUM_LEDS            PIXELS_FOR_CLOCK*NUM_CLOCKS
#define POINT_TIMEOUT       4000
#define MIN_BRITNESS        30
#define SETUP_RESET_TIMEOUT 10000

char timezones[NUM_CLOCKS] = {0,1};
byte britness = 255;

CRGB leds[NUM_LEDS];

#define BUTTON1_PIN 2
#define BUTTON2_PIN 3

#define LED_PIN 13  // built-in LED

bool blinkState = false;

Button button1(BUTTON1_PIN, BUTTON_PULLUP_HIGH);
Button button2(BUTTON2_PIN, BUTTON_PULLUP_HIGH);

byte setup_mode = 0;
#define SETUP_MODES_COUNT 6
#define SETUP_MODE_NONE 0
#define SETUP_MODE_MSK_HOURS 1
#define SETUP_MODE_MSK_MINUTES 2
#define SETUP_MODE_BRITNESS 3
#define SETUP_MODE_TZ_1 4
#define SETUP_MODE_TZ_2 5

void initSerial(){
    Serial.begin(19200);
    while (!Serial); // wait for Leonardo enumeration, others continue immediately
}

void initLEDS(){
    FastLED.addLeds<WS2812, 6, BRG>(leds, NUM_LEDS);
}

void initClock () {
    TimeElements te;
    te.Second = 0;
    te.Minute = 0;
    te.Hour = 12;
    te.Day = 15;
    te.Month = 12;
    te.Year = 2016 - 1970; // lib year starts from 1970
    time_t timeVal = makeTime(te);
    RTC.set(timeVal);
    setTime(timeVal);
}

void setup()  {
    initSerial();
    // Инициализация диодов
    initLEDS();
    britness = EEPROM.read(0);
    if (britness < MIN_BRITNESS) {
        britness = MIN_BRITNESS;
    }
    for (byte i = 1; i < NUM_CLOCKS; i++) {
        timezones[i] = EEPROM.read(i);
    }
    
    setSyncProvider(RTC.get);   // get RTC time
    if (timeStatus() != timeSet)
      Serial.println("Unable to sync with the RTC");
    else
      Serial.println("RTC has set the system time");
}

byte button1_long_press = 0;
byte button2_long_press = 0;

char msk_hours_offset = 0;
char msk_minutes_offset = 0;
int timezone_offset = 0;

unsigned long last_setup_action = 0;

void loop(){
    blinkState = !blinkState;
    digitalWrite(LED_PIN, blinkState);
    process_button_and_setup();
    read_and_print_time();
    process_point();
    FastLED.show();
    delay(5);
//  sprintf(buf, "%02d:%02d:%02d %02d.%02d.%4d", hour(), minute(), second(), day(), month(), year());
}

void process_button_and_setup() {
    byte button1_state = button1.processState();
    byte button2_state = button2.processState();
    if (button1_state == LONG_PRESS) {
        button1_long_press = 1;
    }
    if (button2_state == LONG_PRESS) {
        button2_long_press = 1;
    }

    if (button1_long_press && button2_long_press) {
        last_setup_action = millis();
        button1_long_press = button2_long_press = 0;
        setup_mode = (setup_mode+1) % SETUP_MODES_COUNT;
        if (setup_mode >= SETUP_MODE_MSK_MINUTES) {
          switch (setup_mode) {
            case SETUP_MODE_MSK_MINUTES:
              if (msk_hours_offset != 0) {
                  TimeElements tm;
                  RTC.read(tm);
                  tm.Hour = (tm.Hour + msk_hours_offset + 24) % 24;
                  RTC.write(tm);
                  msk_hours_offset = 0;
              }
              break;
            case SETUP_MODE_BRITNESS:
              if (msk_minutes_offset != 0) {
                  TimeElements tm;
                  RTC.read(tm);
                  tm.Minute = (tm.Minute + msk_minutes_offset + 60) % 60;
                  RTC.write(tm);
                  msk_minutes_offset = 0;
              }
              break;
            case SETUP_MODE_TZ_1:
              EEPROM.update(0, britness);
              timezone_offset = 0;
              break;
            case SETUP_MODE_TZ_2:
              timezones[1] += timezone_offset;
              EEPROM.update(1, timezones[1]);
              timezone_offset = 0;
              break;
            case SETUP_MODE_NONE:
              timezones[10] += timezone_offset;
              EEPROM.update(10, timezones[10]);
              timezone_offset = 0;
              break;
          }
        }
    }
    char action = 0;
    if (button1_state == SHORT_PRESS) {
        action = -1;
    }
    if (button2_state == SHORT_PRESS) {
        action = 1;
    }
    if (action != 0 && setup_mode) {
        last_setup_action = millis();
        switch(setup_mode) {
            case SETUP_MODE_MSK_HOURS:  // Меняем часы москвы
              msk_hours_offset += action;
              break;
            case SETUP_MODE_MSK_MINUTES:  // Меняем минуты москвы
              msk_minutes_offset += action;
              break;
            case SETUP_MODE_BRITNESS:  // Меняем яркость
              if (uint16_t(britness) + int16_t(action*15) > 255) {
                  britness = 255;
              }
              else {
                  britness = britness + action*15;
              }
              if (britness < MIN_BRITNESS) {
                  britness = MIN_BRITNESS;
              }
              break;
            // Меняем таймзоны!
            default:
              timezone_offset = (timezone_offset + action) % 24;
              break;
        }
    }
    if (setup_mode && last_setup_action && millis() - last_setup_action > SETUP_RESET_TIMEOUT) {
        setup_mode = 0;
        last_setup_action = 0;
        timezone_offset = 0;
        msk_hours_offset = 0;
        msk_minutes_offset = 0;
    }
}

void read_and_print_time() {
  TimeElements now;
    if (RTC.read(now)) {
        for (byte i = 0; i < NUM_CLOCKS; i++) {
            print_time(i, (now.Hour + timezones[i] + 24) % 24, now.Minute, setup_mode, CRGB(britness, britness, britness));
        }
    }
    else {
        if (RTC.chipPresent()) {
          // The DS1307 is stopped.  Please run the SetTime
          // example to initialize the time and begin running.
            initClock();
        } else {
            print_time(0, 0, 99, 0, CRGB(250, 250, 0));
        }
    }
}


void process_point (){
    uint16_t state = millis() % POINT_TIMEOUT;
    byte color = 0;
    if (state < (POINT_TIMEOUT/4)) {
        color = byte(255 * (state / float(POINT_TIMEOUT/4.0)));
    }
    else if (state >= (POINT_TIMEOUT/4) && state < POINT_TIMEOUT/2) {
        color = 255;
    }
    else if (state >= (POINT_TIMEOUT/2) && state < (POINT_TIMEOUT*3/4)) {
        color = byte(255 * ((POINT_TIMEOUT*3.0/4) - state) / (POINT_TIMEOUT/4.0));
    }
    else {
        color = 0;
    }

    color = 255;
    for (byte i = 0; i < NUM_CLOCKS; i++) {
        print_point(i, color);
    }
}


void print_time(byte clock_num, byte hours, byte minutes, byte setup_mode, CRGB base_color) {
    CRGB* base_leds = leds + clock_num * PIXELS_FOR_CLOCK;

    // Заливаем текущие часы черным цветом
    for(byte i = 0; i < PIXELS_FOR_CLOCK; i++){
        base_leds[i] = CRGB::Black;
    }

    if (clock_num == 0 && setup_mode == SETUP_MODE_MSK_HOURS) {
        hours = (hours + msk_hours_offset + 24) % 24;
    }

    if (clock_num == 0 && setup_mode == SETUP_MODE_MSK_MINUTES) {
        minutes = (minutes + msk_minutes_offset + 60) % 60;
    }

    if (setup_mode >= SETUP_MODE_TZ_1 && clock_num == (setup_mode - SETUP_MODE_TZ_1 + 1)) {
        hours = (hours + timezone_offset + 24) % 24;
    }
    
    CRGB color;

    if ((clock_num == 0 && (setup_mode == SETUP_MODE_MSK_HOURS || setup_mode == SETUP_MODE_BRITNESS)) ||
        (setup_mode == SETUP_MODE_TZ_1 && clock_num == 1) ||
        (setup_mode == SETUP_MODE_TZ_2 && clock_num == 2)) {
        color = CRGB(0, britness, 0);
    }
    else {
        color = base_color;
    }

    if (setup_mode == SETUP_MODE_BRITNESS) {
      print_digit(base_leds, 3, 0, color);
      print_digit(base_leds, 2, britness / 100, color);
      print_digit(base_leds, 1, (britness / 10) % 10, color);
      print_digit(base_leds, 0, britness % 10, color);
      return;
    }
    
    print_digit(base_leds, 3, hours / 10, color);
    print_digit(base_leds, 2, hours % 10, color);

    if (clock_num == 0 && (setup_mode == 2 || setup_mode == 3)) {
      color = CRGB(0, britness, 0);
    }
    else {
      color = base_color;
    }
    print_digit(base_leds, 1, minutes / 10, color);
    print_digit(base_leds, 0, minutes % 10, color);
}


#define MAX_SEGMENTS 7
int digit_to_segments [10][MAX_SEGMENTS]= {
    {0, 0, 1, 2, 3, 4, 5},      // 0
    {0, 0, 0, 0, 0, 0, 5},      // 1
    {0, 0, 0, 1, 6, 3, 4},      // 2
    {0, 0, 0, 1, 6, 5, 4},      // 3
    {0, 0, 0, 0, 2, 6, 5},      // 4
    {1, 1, 1, 2, 6, 5, 4},      // 5
    {1, 1, 2, 3, 4, 5, 6},      // 6
    {0, 0, 0, 0, 0, 1, 5},      // 7
    {0, 1, 2, 3, 4, 5, 6},      // 8
    {0, 0, 1, 2, 4, 5, 6}       // 9
};


int segments_to_pixel [4][7]= {
    {6, 7, 8, 3, 4, 5, 28},
    {9, 10, 11, 0, 1, 2, 27},
    {24, 23, 22, 15, 14, 13, 26},
    {21, 20, 19, 18, 17, 16, 25}
};

int point_to_pixel = {12};

void print_point(byte clock_num, byte color) {
    CRGB* base_leds = leds + clock_num * PIXELS_FOR_CLOCK;
    color = (color * britness/255.0);
    base_leds[point_to_pixel] = CRGB(color, color, color);
}

void print_digit(CRGB* base_leds, byte digit_num, byte digit_value, CRGB color) {
    byte i = 0;

    // digit_to_segments[digit_value] - Таким образом получаем список сегментов, которые надо зажеч
    for (i = 0; i < MAX_SEGMENTS; i++) {
        byte segment_num = digit_to_segments[digit_value][i];       // Получаем сегмент, который надо зажечь
        byte pixel_num = segments_to_pixel[digit_num][segment_num];  // Получаем пиксель, который надо зажечь
        base_leds[pixel_num] = color;
    }
}

