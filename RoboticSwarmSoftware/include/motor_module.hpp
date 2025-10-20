#ifndef MOTOR_MODULE_HPP
#define MOTOR_MODULE_HPP

#include <Arduino.h>
#include "globals.hpp"

void initMotors(uint8_t step1, uint8_t step2, uint8_t step3, uint8_t dir1, uint8_t dir2, uint8_t dir3);

//Emergency Stop
void stopMotors();

//For override manual controls from hub
void setMotorSteps(int leftSteps, int rightSteps, int backSteps);

// FreeRTOS Task
void motorTask(void* parameter);

#endif