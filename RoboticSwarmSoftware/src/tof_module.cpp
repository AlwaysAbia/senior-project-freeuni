#include "tof_module.hpp"
#include "globals.hpp"
#include <VL53L0X.h>
#include <Wire.h>

#define TCA_ADDR 0x70         // Default address of TCA9548A
#define SENSOR_COUNT 6
#define SENSOR_UPDATE_MS 5    // Update interval per sensor

VL53L0X sensor[SENSOR_COUNT];
bool sensorInitialized[SENSOR_COUNT] = {false};

//-----------------------------------------
// Helper Functions
void tcaSelect(uint8_t channel) {
    if (channel > 7) return;

    Wire.beginTransmission(TCA_ADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

void tcaDeselectAll() {
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(0);
    Wire.endTransmission();
}

int readDistance(uint8_t channel) {
    if (channel >= SENSOR_COUNT) return -1; // Invalid Channel Error
    if (!sensorInitialized[channel]) return -2; // Sensor not initialized

    tcaSelect(channel);
    return sensor[channel].readRangeContinuousMillimeters();
}

//----------------------------------------
// Setup and FreeRTOS Task

void initAllToFSensors() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    for (uint8_t i = 0; i < SENSOR_COUNT; ++i) {
        tcaSelect(i);

        if (sensor[i].init()) { 
            sensor[i].setTimeout(500);
            sensor[i].startContinuous(20);
            sensorInitialized[i] = true;
        } else {
            Serial.print("ToF sensor init failed on channel ");
            Serial.println(i);
        }
    }
}

void TOFsensorTask(void* parameter) {
    uint8_t currentSensor = 0;

    while (true) {
        if (sensorInitialized[currentSensor]) {
            int distance = readDistance(currentSensor);

            // Update state with mutex protection
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                state.distances[currentSensor] = distance;
                xSemaphoreGive(stateMutex);
            }
        }

        currentSensor = (currentSensor + 1) % SENSOR_COUNT;
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_MS));
    }
}
