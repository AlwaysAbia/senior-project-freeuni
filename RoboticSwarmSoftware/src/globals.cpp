#include "globals.hpp"

State state = { State::OFF, 0, 0, 0, 0, 0, 0, 0, {0}};
int tof_ch_order[6] = {2, 1, 0, 5, 4, 3}; 
int ir_ch_order[6] = {0, 1, 2, 3, 4, 5};

SemaphoreHandle_t stateMutex;