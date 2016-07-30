#define RF24_ADDR 0xFFFF000002

#define DHTPIN 2
#define DHTTYPE DHT11
#define WATCH_DOG_SLEEP

#include "DHT.h"

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SPI.h>
#include "printf.h"
#include "Some.h"

Thing<link_RF24> me(0,
                    "{\"name\":\"温湿度传感器\",\"groups\":{\"smart\":{\"alwaysOn\":true,\"heartBeat\":false},\"air-th\":{}}}");

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  //Serial.begin(115200);
  //printf_begin();
  //Serial.println("setup!");
  me.setup();
  dht.begin();
  
  // disable ADC
  // works better when set during setup?
  ADCSRA = 0;
}

void init_loop(void) {
  static int loop_time_in_second = 5;
  const static int loop_gap_in_msecond = 10;
  while (loop_time_in_second > 0) {
    for (int ms = 0; ms < 1000; ms += loop_gap_in_msecond) {
      delay(loop_gap_in_msecond);
      me.refresh();
    }
    loop_time_in_second--;
  }
}

void loop(void) {
  init_loop();
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int hd = (int)h;
  int hp = (int)(h * 10.0) % 10;
  int td = (int)t;
  int tp = (int)(t * 10.0) % 10;
  if (isnan(h) || isnan(t)) {
    //Serial.println("Failed to read from DHT sensor!");
  }

  char buff[64];
  sprintf((char*)buff, "{\"temp\":%d.%d,\"hum\":%d.%d}", td, tp, hd, hp);

  me.send(buff);
  me.deep_sleep(2800000); //sleep 46 min
}

