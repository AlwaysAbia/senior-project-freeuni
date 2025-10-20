#include <Arduino.h>

#include "tof_module.hpp"
#include "ir_module.hpp"
#include "motor_module.hpp"
#include "network_module.hpp"
#include "globals.hpp"

// Task handles for control
TaskHandle_t motorTaskHandle = NULL;
TaskHandle_t TOFsensorTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

TaskHandle_t IRsensorTaskHandle = NULL; //Not-Implemented

void setup() {
  Serial.begin(115200);
  
  stateMutex = xSemaphoreCreateMutex();
  
  if (stateMutex == NULL) {
    Serial.println("Failed to create mutex!");
    while (1) delay(1000);
  }

  connectToHub();
  setupOTA();
  setupServer();

  initMotors(STPR_STEP_1, STPR_STEP_2, STPR_STEP_3, STPR_DIR_1, STPR_DIR_2, STPR_DIR_3);
  initAllToFSensors();

  state.mode = State::OFF;

 // Create tasks pinned to specific cores
  // Core 1 for time-critical motor control
  xTaskCreatePinnedToCore(
    motorTask,           // Task function
    "MotorTask",         // Name
    4096,                // Stack size
    NULL,                // Parameters
    3,                   // Priority (high)
    &motorTaskHandle,    // Task handle
    1                    // Core 1
  );
  
  // Core 1 for sensors (medium priority)
  xTaskCreatePinnedToCore(
    TOFsensorTask,
    "TOFSensorTask",
    4096,
    NULL,
    2,                   // Priority (medium)
    &TOFsensorTaskHandle,
    1                    // Core 1
  );
  
  // Core 0 for network (lower priority, won't interfere with motors)
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,                // Larger stack for network operations
    NULL,
    1,                   // Priority (low)
    &networkTaskHandle,
    0                    // Core 0 (where WiFi runs)
  );
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}