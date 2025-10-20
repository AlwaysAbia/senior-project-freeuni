#ifndef IR_MODULE_HPP
#define IR_MODUE_HPP

#include <Arduino.h>
#include <Wire.h>

//Using RMT Peripheral, need to call setupIR outside to setup analog Mux
void setupIR(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t tx_pin, uint8_t rx_pin); 

//FreeRTOS Task
void IRsensorTask(void* parameter);

#endif