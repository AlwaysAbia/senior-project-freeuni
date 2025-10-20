#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <Arduino.h>

#define I2C_SDA_PIN 10
#define I2C_SCL_PIN 11

#define IR_Rx 35
#define IR_Tx 36
#define IR_MUX_S0 37
#define IR_MUX_S1 38
#define IR_MUX_S2 39

#define STPR_DIR_1 16
#define STPR_STEP_1 5

#define STPR_DIR_2 17
#define STPR_STEP_2 6

#define STPR_DIR_3 18
#define STPR_STEP_3 7

#define LED_PIN     9
#define NUM_LEDS    1
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

struct State {
  enum Mode {
    OFF,
    IDLE,
    LINE,
    POLYGON,
    MANUAL
  } mode;

  // --- ID / General config ---
  uint16_t neighbor_maxDist;   // Max distance (cm/mm) to consider another robot a "neighbor"

  // --- Idle mode ---
  uint16_t idle_thresh;        // Distance threshold for detecting idle activity

  // --- Line formation parameters ---
  uint16_t line_nodeDist;      // Desired spacing between nodes
  uint16_t line_alignTol;      // Distance tolerance for being "in position"

  // --- Polygon formation parameters ---
  uint8_t  polygon_sides;      // Number of robots / polygon vertices
  uint16_t polygon_radius;     // Target radius of polygon
  uint16_t polygon_alignTol;   // Allowed deviation from perfect radius

  // --- Sensor data (read-only snapshot) ---
  uint32_t distances[6];       // IR / ToF readings (filled by sensor module)
};


extern State state;
extern int tof_ch_order[6];
extern int ir_ch_order[6];

// FreeRTOS Mutex
extern SemaphoreHandle_t stateMutex;

#endif