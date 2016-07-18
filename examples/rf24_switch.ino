#define RF24_ADDR 0xFFFF000001
//#define SOME_DEBUG
//#define SOME_ERROR
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SPI.h>
#include "printf.h"
#include "Some.h"

Thing<link_RF24> me(0,
                    "{\"name\":\"智能灯\",\"groups\":{\"power\":{\"hasBattery\":false},\"smart\":{\"alwaysOn\":true,\"heartBeat\":false},\"light\":{}}}");


void turnOn(void) {
  Serial.println("turnOn");
  digitalWrite(6, LOW);
  me.send("{\"power\":1}");
}

void turnOff(void) {
  Serial.println("turnOff");
  digitalWrite(6, HIGH);
  me.send("{\"power\":0}");
}

void setup() {
//  Serial.begin(115200);
//  printf_begin();
  pinMode(6, OUTPUT);
  digitalWrite(6, LOW); //Turn the light on as soon as possible.
  me.setup();
  me.poke();
  me.on("turnOn", turnOn);
  me.on("turnOff", turnOff);
  turnOn();
}

void loop(void) {
  delay(50);
  me.refresh();
}

