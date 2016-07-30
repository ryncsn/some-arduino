// Copyright (c) 2016
// Author: 

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SOME_H
#define SOME_H

//#define SOME_DEBUG
//#define SOME_ERROR
#define SOME_OPRATION_BUFF 128;

#ifdef SOME_DEBUG
#define DPrint(x) Serial.print(x)
#define DPrintln(x) Serial.println(x)
#else
#define DPrint(x)
#define DPrintln(x)
#endif

#ifdef SOME_ERROR
#define EPrint(x) Serial.print(x)
#define EPrintln(x) Serial.println(x)
#else
#define EPrint(x)
#define EPrintln(x)
#endif

#ifndef RF24_HOST_ADDR
#define RF24_HOST_ADDR 0xABCD000000
#endif

#ifndef RF24_ADDR
#define RF24_ADDR 0xFFFF000000
//#error
#endif

#ifndef RF24_CE
#define RF24_CE 7
#endif

#ifndef RF24_CS
#define RF24_CS 8
#endif

#define RF24_MAX_LEN 32  //NRF24L01 payload length
#define RF24_BUFF_LEN 256 //Buffer for transimission

#define BUFF_LEN 256 //Buffer for transmission

#ifndef NOEEPROM
#include <EEPROM.h>
#define ID_ADDRESS 0
#endif

#include <SPI.h>
#include <ArduinoJson.h>
#include "RF24.h"


#ifndef WD_SLEEP_MS
#define WD_SLEEP_MS 8000
#endif

#if(WD_SLEEP_MS == 500)
#define WDT_REG bit (WDIE) | bit (WDP2) | bit (WDP0);
#endif
#if(WD_SLEEP_MS == 4000)
#define WDT_REG bit (WDIE) | bit (WDP3);
#endif
#if(WD_SLEEP_MS == 8000)
#define WDT_REG bit (WDIE) | bit (WDP3) | bit (WDP0);
#endif

#ifdef WATCH_DOG_SLEEP
#include <avr/wdt.h>
#include <avr/sleep.h>

//Watchdog timer, always in RAM
volatile uint32_t _wd_count = 0;

ISR (WDT_vect) {
	_wd_count ++;
}
#endif

static const byte RF24_END[RF24_MAX_LEN] = {0};

typedef void (*HandleFunction)(void);
class OpHandler {
	public:
		const char* op;
		HandleFunction func;
		OpHandler *next;

	public:
		OpHandler(const char* op, HandleFunction func):
			op(op),
			func(func),
			next(NULL)
		{
		}
};


template <typename Link>
class Thing {
	private:
		uint16_t _id;
		Link* _link;
		const char* _desc;
		OpHandler *_first_handler, *_last_handler = NULL;

	private:
		bool _apply(const JsonObject& root){
			const char* op = root["op"];
			DPrint("Apply op: ");
			DPrintln(op);
			if (strncmp(op, "init", sizeof("init") + 1) == 0){
				DPrintln("Init request");
				this -> poke();
				return true;
			}
			if (strncmp(op, "setid", sizeof("setid") + 1) == 0){
				if(!root.containsKey("id")){
					return false;
				}
				DPrint("Set id: ");
				setid((int)root["id"]);
				DPrintln(_id);
				return false;
			}

			DPrintln("Try to find handler");
			OpHandler* handler = _first_handler;
			while(handler != NULL){
				if (strcmp(handler->op, op) == 0){
					DPrintln("Found handler");
					handler->func();
					return true;
				}
				handler = handler -> next;
			}

			DPrintln("No handler, abort.");
			return false;
		}

	public:

		Thing(uint16_t id, const char* desc):
			_link(new Link(this)),
			_id(id),
			_desc(desc)
		{
		}

		//TODO: destructor, may not needed

		void setup(){
			_link -> setup();
#ifndef NOEEPROM
			readid();
			DPrintln("Read id");
#endif
			DPrint("Thing init, id:");
			DPrintln(_id);
			DPrint("desc:");
			DPrintln(_desc);
		}

		bool on(const char* op, HandleFunction func){
			if (_first_handler == NULL)
				_first_handler = _last_handler = new OpHandler(op, func);
			else
				_last_handler = _last_handler -> next = new OpHandler(op, func);
			DPrint("Add hanler for ");
			DPrintln(op);
			return true;
			//TODO: return false on invalid op
		}

		void poke(){
			this -> send(_desc);
		}

		void setid(uint16_t id){
			this -> _id = id;
#ifndef NOEEPROM
			saveid();
			DPrintln("Save id");
#endif
		}

		bool send(const char* str, uint32_t len){
			//str should be a c style string, end with \0
			//len shoule be ret of strlen without \0
			char buffer[BUFF_LEN];
			int i = 0, skip;

			//skip is the length of the chars we printed without \0
			skip = sprintf(buffer, "{\"id\":%d,", _id);
			if(len > BUFF_LEN - skip)
				return false;

			//++i for str to skip the first '{'
			while (i < len){
				buffer[skip + i] = str[++i];
			}

			//str's \0 was copied, cut it by - 1
			_link -> send(buffer, i + skip - 1);
		}

		bool send(const char* str){
			return this -> send(str, strlen(str));
		}

		bool refresh(){
			int read_len = _link->receive();
			if(read_len == 0) {
				return 0;
			}

			StaticJsonBuffer<BUFF_LEN> jsonBuffer;
			JsonObject& root = jsonBuffer.parseObject((char*)_link->getBuffer());
			if (!root.success() || !root.containsKey("op"))
			{
				EPrintln("No JSON");
				return false;
			}
			this->_apply(root);
			return true;
		}

		void sleep(){
			_link -> sleep();
		}

#ifdef WATCH_DOG_SLEEP
		//Sleep in ms.
		void deep_sleep(uint32_t time){
			_link -> sleep();
			uint32_t target_time = (time + WD_SLEEP_MS - 1) / WD_SLEEP_MS; //Up Rounding

			ADCSRA = 0;
			// clear various "reset" flags
			MCUSR = 0;
			// allow changes, disable reset
			WDTCSR = bit (WDCE) | bit (WDE);
			// set interrupt mode and an interval
			WDTCSR = WDT_REG;    // set WDIE, and 8 second delay
			wdt_reset();  // pat the dog

			set_sleep_mode (SLEEP_MODE_STANDBY);

			noInterrupts();           // timed sequence follows
			sleep_enable();

			// turn off brown-out enable in software
			MCUCR = bit (BODS) | bit (BODSE);
			MCUCR = bit (BODS);
			interrupts ();             // guarantees next instruction executed

			_wd_count = 0;
			while(_wd_count < target_time){
				sleep_cpu (); //Sleep and wait for Interuput
			}
			wdt_disable();

			// cancel sleep as a precaution
			sleep_disable();
			this->wakeup();
		}
#endif

		void wakeup(){
			_link -> wakeup();
		}

#ifndef NOEEPROM
		void saveid() {
			byte lowByte = ((_id >> 0) & 0xFF);
			byte highByte = ((_id >> 8) & 0xFF);
			EEPROM.write(ID_ADDRESS, lowByte);
			EEPROM.write(ID_ADDRESS + 1, highByte);
		}

		void readid() {
			byte lowByte = EEPROM.read(ID_ADDRESS);
			byte highByte = EEPROM.read(ID_ADDRESS + 1);
			_id = ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
		}
#endif
};

class link_RF24 {
	private:
		RF24 _radio;
		Thing<link_RF24>* _thing;
		byte _buffer[RF24_BUFF_LEN];

	public:
		int _buffer_len;

	public:
		link_RF24(Thing<link_RF24>* thing):
			_radio(RF24_CE, RF24_CS),
			_thing(thing),
			_buffer({0})
		{
			_buffer_len = 0;
		};

		void setup(){
			_radio.begin();
			_radio.enableAckPayload();			//EN_ACK_PAY
			_radio.enableDynamicAck();			//EN_DYN_ACK
			_radio.enableDynamicPayloads();		//EN_DPL

			//Setup channel
			_radio.setChannel(100);
			_radio.setDataRate(RF24_250KBPS);
			_radio.setPALevel(RF24_PA_MAX);
			_radio.setCRCLength(RF24_CRC_16);
			_radio.setRetries(15, 15);			//First 15 means 4000us, second means 15 times

			_radio.openReadingPipe(1, RF24_ADDR);
			_radio.openWritingPipe(RF24_HOST_ADDR);

			_radio.startListening();
		}

		byte* getBuffer(){
			return _buffer;
		}

		uint32_t receive(){
			while(_radio.available()){
				uint8_t dpl_len = _radio.getDynamicPayloadSize();
				if(dpl_len < 1){
					DPrintln("RF24 receive: Empty package, skip.");
					return 0;
				}

				//Remaining buffer have to be able to contain this payload plus
				//an END package, unless this package is a END package.
				if (dpl_len > RF24_BUFF_LEN - _buffer_len - sizeof(RF24_END)
						&& dpl_len != sizeof(RF24_END)) {
					EPrintln("RF24 receive: Buffer Overflow!");
					return 0;
				}

				_radio.read(_buffer + _buffer_len, dpl_len);
				_buffer_len += dpl_len;
				if (dpl_len == sizeof(RF24_END)
						&& memcmp(_buffer + _buffer_len, RF24_END, sizeof(RF24_END)) == 0){
					DPrintln("RF24 receive: Got end package.");
					uint32_t len = _buffer_len;
					_buffer_len = 0;
					return len;
				}
				DPrintln("RF24 receive: Got package.");
			}
			return 0;
		}

		uint32_t receive(void* buf, uint32_t buf_len){
			//TODO
			return receive();
		}

		bool send(void* buf, uint32_t buf_len){
			int remain = buf_len;
			bool issent = !remain;

			DPrintln("Begin Tranmission");
			_radio.stopListening();
			_radio.flush_tx();

			//Send a END in case of privious error or device not available
			if (!_radio.write(RF24_END, sizeof(RF24_END))) {
				DPrintln("Tranmission failed to start (tring to write first END).");
				_radio.startListening();
				return -1;
			}

			while (remain >= 0) {
				if (_radio.write(buf, (RF24_MAX_LEN > remain) ? remain : RF24_MAX_LEN)) {
					remain -= RF24_MAX_LEN;
					buf += RF24_MAX_LEN;
				}
				else {
					DPrintln("Failure during transimiting, aborting.");
					_radio.startListening();
					return -1;
				}
			}

			if (_radio.write(RF24_END, sizeof(RF24_END))) {
				DPrintln("Tranmission ended successfully.");
				_radio.startListening();
				return 0;
			}

			DPrintln("Tranmission failed to end (tring to last first END).");
			_radio.startListening();
			return -1;
		}

		void sleep(){
			_radio.setPALevel(RF24_PA_MIN);
			_radio.stopListening();
			_radio.powerDown();
		}

		void wakeup(){
			_radio.powerUp();
			_radio.startListening();
			_radio.setPALevel(RF24_PA_MIN);
		}
};

#endif
