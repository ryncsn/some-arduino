#define RF24_ADDR 0xFFFF000001

#include <ArduinoJson.h>
#include <EEPROM.h>
#include <SPI.h>
#include "printf.h"
#include "Some.h"

Thing<link_RF24> me(0,
        "{\"name\":\"智能灯\",\"groups\":{\"power\":{\"hasBattery\":false},\"smart\":{\"alwaysOn\":true,\"heartBeat\":false},\"light\":{}}}");


void turnOn(void) {
    Serial.println("turnOn");
    digitalWrite(13, HIGH);
    me.send("{\"power\":1}");
}

void turnOff(void) {
    Serial.println("turnOff");
    digitalWrite(13, LOW);
    me.send("{\"power\":0}");
}

void setup() {
    Serial.begin(115200);
    printf_begin();
    me.setup();
    me.poke();
    me.on("turnOn", turnOn);
    me.on("turnOff", turnOff);
    pinMode(13, OUTPUT);
}

void loop(void) {
    delay(50);
    me.refresh();
}

