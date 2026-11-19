/*

espurna-base: what a library of ESPurna firmware includes instead of <Arduino.h>.

The scheme is the one of ESPurna before 2026-11 (Arduino): the library includes one header of its platform and calls
millis(), pinMode(), attachInterrupt() ...; the firmware implements them. Here the header is this one and the firmware
is ESPurna (lib/espurna-core/src/wiring.cpp and espurna_base.cpp: the same C symbols ArduinoCore-API declares, so the
values below are ArduinoCore-API's). The library sees nothing of the firmware and builds on its own.

- String: ESPurna's own small string (espurna_string.h, part of this library)
- the calls: plain C declarations

*/

#pragma once

// the C headers every Arduino.h gives
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "espurna_string.h"

// the interrupt handlers of the library: in IRAM where the chip has one (ESP8266, ESP32)
#if __has_include(<esp_attr.h>)
#include <esp_attr.h>
#endif
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

typedef ::espurna::string String;
typedef uint8_t byte;

// the values of ArduinoCore-API (PinStatus, PinMode)
#define LOW 0
#define HIGH 1
#define CHANGE 2
#define FALLING 3
#define RISING 4

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define INPUT_PULLDOWN 0x3
#define OUTPUT_OPENDRAIN 0x4

#define DEC 10
#define HEX 16

// no separate flash strings
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef F
#define F(x) (x)
#endif
#ifndef FPSTR
#define FPSTR(x) (x)
#endif
#ifndef PSTR
#define PSTR(x) (x)
#endif
#ifndef PGM_P
#define PGM_P const char*
#endif
#ifndef strlen_P
#define strlen_P strlen
#endif
#ifndef strcasecmp_P
#define strcasecmp_P strcasecmp
#endif
#ifndef snprintf_P
#define snprintf_P snprintf
#endif

#ifdef __cplusplus
template <typename T, typename L, typename H>
inline T constrain(T value, L low, H high) {
    return (value < low) ? static_cast<T>(low) : ((value > high) ? static_cast<T>(high) : value);
}

extern "C" {
#endif

// time: since the start, wraps around
unsigned long millis(void);
unsigned long micros(void);

// sleep, the network keeps running
void delay(unsigned long ms);

// busy wait
void delayMicroseconds(unsigned int us);

// pins: `mode` INPUT ... OUTPUT_OPENDRAIN, `level` LOW / HIGH
void pinMode(uint8_t pin, int mode);
void digitalWrite(uint8_t pin, int level);
int digitalRead(uint8_t pin);

// an interrupt on CHANGE / FALLING / RISING; `handler` runs in the interrupt (IRAM_ATTR)
void attachInterrupt(uint8_t pin, void (*handler)(void), int mode);
void detachInterrupt(uint8_t pin);

void noInterrupts(void);
void interrupts(void);

// the chip restarts
void espurna_restart(void) __attribute__((noreturn));

// the station: IPv4 address a.b.c.d, all zero without one; MAC
void espurna_local_ip(uint8_t out[4]);
void espurna_mac(uint8_t out[6]);

// MD5 of `size` bytes
void espurna_md5(const void* data, size_t size, uint8_t out[16]);

#ifdef __cplusplus
} // extern "C"
#endif
