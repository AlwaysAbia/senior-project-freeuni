#ifndef TOF_MODULE_HPP
#define TOF_MODULE_HPP

#include <Arduino.h>
#include <Wire.h>

void initAllToFSensors();

//FreeRTOS Task
void TOFsensorTask(void* parameter);

#endif