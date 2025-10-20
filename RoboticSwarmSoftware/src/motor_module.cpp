#include <AccelStepper.h>

#include "motor_module.hpp"
#include "globals.hpp"

AccelStepper* stepperleft = nullptr;
AccelStepper* stepperright = nullptr;
AccelStepper* stepperback = nullptr;

//-----------------------------------------------
// Init Functions
void setupMotorVar(AccelStepper*& stp, uint8_t step_pin, uint8_t dir_pin, int max_speed, int accel){
    stp = new AccelStepper(AccelStepper::DRIVER, step_pin, dir_pin); // STEP, DIR
    stp->setMaxSpeed(max_speed);
    stp->setAcceleration(accel);
}

void initMotors(uint8_t step1, uint8_t step2, uint8_t step3, uint8_t dir1, uint8_t dir2, uint8_t dir3){
    setupMotorVar(stepperright, step1, dir1, 300, 4000);
    setupMotorVar(stepperleft, step2, dir2, 300, 4000);
    setupMotorVar(stepperback, step3, dir3, 300, 4000);
}

//------------------------------------------------
// Basic Move Functions
void setMotorSteps(int leftSteps, int rightSteps, int backSteps){
    if (stepperleft) stepperleft->move(-leftSteps); //Pos Forward - Neg Backward
    if (stepperright) stepperright->move(rightSteps); //Pos Forward - Neg Backward
    if (stepperback) stepperback->move(backSteps); //Pos Right - Neg Left
}

void moveMotors(){
    if(stepperleft) stepperleft->run();
    if(stepperright) stepperright->run();
    if(stepperback) stepperback->run();
}

void stopMotors(){
    if(stepperleft) stepperleft->stop();
    if(stepperright) stepperright->stop();
    if(stepperback) stepperback->stop();
}

void moveTowardsSensori(int i, int steps){
    switch (i){
        case 0:
            setMotorSteps(steps,0,steps);
            break;
        case 1:
            setMotorSteps(0, -steps, steps);
            break;
        case 2:
            setMotorSteps(-steps,-steps,0);
            break;
        case 3:
            setMotorSteps(-steps, 0, -steps);
            break;
        case 4:
            setMotorSteps(0, steps, -steps);
            break;
        case 5:
            setMotorSteps(steps,steps,0);
            break;
        default:
            setMotorSteps(0, 0, 0);
            break;
    }
}

void spinClockwise(int degrees){
    int steps = (degrees * 14) / 9;
    setMotorSteps(steps, -steps, -steps);
}

//-----------------------------------------------
// Helper Functions

uint8_t getSensorMask_Idle(State* state) {
    uint8_t mask = 0;
    int distances[6];  // Local copy of distances
    int idle_thresh;   // Local copy of threshold

    // Acquire mutex to snapshot the data
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {  // Slightly longer timeout; adjust as needed
        
        for (int i = 0; i < 6; i++) {
            distances[i] = state->distances[i];
        }

        idle_thresh = state->idle_thresh;

        xSemaphoreGive(stateMutex);
    } else {
        // Handle failure: e.g., assume all blocked or log error
        return 0x3F;  // 0b111111, all sensors blocked as a safe default
    }

    // Now compute safely using local copies (no race possible here)
    for (int i = 0; i < 6; i++) {
        if (distances[i] < idle_thresh) {
            mask |= (1 << i);
        }
    }
    return mask;
}

int getBestMoveDirection_Idle(uint8_t blockedMask) {
    int maxFreeLen = 0;
    int startIdx = -1;
    int freeLen = 0;
    int n = 6; // number of sensors

    // loop circularly
    for(int i = 0; i < n * 2; i++) {
        int idx = i % n;
        if (!(blockedMask & (1 << idx))) {
            freeLen++;
            if(freeLen > maxFreeLen) {
                maxFreeLen = freeLen;
                startIdx = idx - freeLen + 1;
            }
        } else {
            freeLen = 0;
        }
    }

    // pick center of largest free segment
    return ((startIdx + maxFreeLen / 2) % n + n) % n;
}

int getBestMoveDirection_Line(State* state) {
    uint8_t mask = 0;
    int distances[6];
    int neighbor_maxDist, nodeDist, alignTol;

    // Snapshot the state under mutex
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return -1; // Safe default
    }
    for (int i = 0; i < 6; i++) distances[i] = state->distances[i];
    neighbor_maxDist = state->neighbor_maxDist;
    nodeDist = state->line_nodeDist;
    alignTol = state->line_alignTol;
    xSemaphoreGive(stateMutex);

    // Find the two closest sensors under threshold
    int first = -1, second = -1;
    for (int i = 0; i < 6; i++) {
        if (distances[i] >= neighbor_maxDist) continue;
        if (first == -1 || distances[i] < distances[first]) {
            second = first;
            first = i;
        } else if (second == -1 || distances[i] < distances[second]) {
            second = i;
        }
    }

    if (first != -1) mask |= (1 << first);
    if (second != -1) mask |= (1 << second);

    // Collect indices of obstacles from mask
    int indices[2] = {-1, -1};
    int count = 0;
    for (int i = 0; i < 6 && count < 2; i++) {
        if (mask & (1 << i)) indices[count++] = i;
    }

    // Decide best move
    if (count == 1) {
        // Single obstacle (endpoint)
        int idx = indices[0];
        if (distances[idx] > nodeDist + alignTol) {
            // Too far, move toward neighbor
            return idx;
        } else if (distances[idx] < nodeDist - alignTol) {
            // Too close, move away from neighbor
            return (idx + 3) % 6;
        } else {
            // Within tolerance, stay still
            return -2;
        }
    } 
    else if (count == 2) {
        int a = indices[0], b = indices[1];
        int angularSep = abs(a - b);
        if (angularSep > 3) angularSep = 6 - angularSep; // Handle wrap-around
        
        if (angularSep == 3) {
            // Two opposite obstacles (good line formation)
            bool aInTolerance = (distances[a] >= nodeDist - alignTol && distances[a] <= nodeDist + alignTol);
            bool bInTolerance = (distances[b] >= nodeDist - alignTol && distances[b] <= nodeDist + alignTol);
            
            if (aInTolerance && bInTolerance) {
                // Both distances good, stay still
                return -2;
            } else {
                // Move toward the farther one
                int farther = (distances[a] >= distances[b]) ? a : b;
                return farther;
            }
        } else {
            // Two non-opposite obstacles: move to middle to realign
            return (a + b) / 2;
        }
    }

    return -1; // No obstacles
}

int getBestMoveDirection_Polygon(State* state) {
    int distances[6];
    int neighbor_maxDist, radius, alignTol, sides;

    // Snapshot the state under mutex
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return -1;
    }
    for (int i = 0; i < 6; i++) distances[i] = state->distances[i];
    neighbor_maxDist = state->neighbor_maxDist;
    radius = state->polygon_radius;
    alignTol = state->polygon_alignTol;
    sides = state->polygon_sides;
    xSemaphoreGive(stateMutex);

    // Find the (sides - 1) closest neighbors under threshold
    int first = -1, second = -1;
    for (int i = 0; i < 6; i++) {
        if (distances[i] >= neighbor_maxDist) continue;
        if (first == -1 || distances[i] < distances[first]) {
            second = first;
            first = i;
        } else if (second == -1 || distances[i] < distances[second]) {
            second = i;
        }
    }

    // ========== CASE: 0 Neighbors ==========
    if (first == -1) {
        return -1; // Search mode
    }

    // ========== CASE: 1 Neighbor ==========
    if (second == -1) {
        int idx = first;
        
        if (distances[idx] > radius + alignTol) {
            return idx; // Too far, move toward
        } else if (distances[idx] < radius - alignTol) {
            return (idx + 3) % 6; // Too close, move away
        } else {
            return -2; // Distance good, stable pair - STOP
        }
    }

    // ========== CASE: 2 Neighbors ==========
    int a = first, b = second;
    
    // Calculate angular separation
    int angularSep = abs(a - b);
    if (angularSep > 3) angularSep = 6 - angularSep;
    
    // Check if neighbors are adjacent (correct for triangle)
    if (angularSep == 1) {
        // === Angles CORRECT ===
        
        bool aInTolerance = (distances[a] >= radius - alignTol && 
                             distances[a] <= radius + alignTol);
        bool bInTolerance = (distances[b] >= radius - alignTol && 
                             distances[b] <= radius + alignTol);
        
        if (aInTolerance && bInTolerance) {
            return -2; // Perfect triangle - STOP
        }
        
        // Angles correct but distances wrong
        bool aTooFar = distances[a] > radius + alignTol;
        bool bTooFar = distances[b] > radius + alignTol;
        bool aTooClose = distances[a] < radius - alignTol;
        bool bTooClose = distances[b] < radius - alignTol;
        
        if ((aTooFar && bTooFar) || (aTooClose && bTooClose)) {
            // Both errors in same direction - alternating motion
            static int toggleCounter = 0;
            const int TOGGLE_PERIOD = 50;
            
            toggleCounter++;
            if (toggleCounter >= TOGGLE_PERIOD * 2) {
                toggleCounter = 0;
            }
            
            bool useFirstNeighbor = (toggleCounter < TOGGLE_PERIOD);
            
            if (aTooFar && bTooFar) {
                // Both too far
                return useFirstNeighbor ? a : b;
            } else {
                // Both too close
                int target = useFirstNeighbor ? a : b;
                return (target + 3) % 6;
            }
        } else {
            // Mixed errors - prioritize larger error
            int errorA = abs(distances[a] - radius);
            int errorB = abs(distances[b] - radius);
            
            if (errorA > errorB) {
                if (aTooFar) return a;
                else return (a + 3) % 6;
            } else {
                if (bTooFar) return b;
                else return (b + 3) % 6;
            }
        }
    } else if (angularSep == 2) {
        // === 1 Sensor Gap ===
        // Find gap sensor and move opposite to it
        int gapSensor = (a + b) / 2;
        if (abs(a - b) > 3) {
            // Handle wrap-around
            gapSensor = ((a + b + 6) / 2) % 6;
        }
        return (gapSensor + 3) % 6; // Move opposite to gap
        
    } else if (angularSep == 3) {
        // === 2 Sensor Gap (Line Formation) ===
        // We're in the middle of a line, move perpendicular
        // Choose one of two perpendicular directions consistently
        return (a + 1) % 6;
        
    } else {
        // Shouldn't happen, but fallback to moving toward midpoint
        int midpoint = (a + b) / 2;
        if (abs(a - b) > 3) {
            midpoint = ((a + b + 6) / 2) % 6;
        }
        return midpoint;
    }
}

//---------------------------------------------
// State Handlers
void handleIdle(State *state, int stepsToScoot) {
    uint8_t blockedMask = getSensorMask_Idle(state);
    int numBlocked = __builtin_popcount(blockedMask);

    if(numBlocked == 0 || numBlocked == 6) {
        setMotorSteps(0, 0, 0);
    } else {
        int moveDir = getBestMoveDirection_Idle(blockedMask);
        moveTowardsSensori(moveDir, stepsToScoot);
    }
}

void handleLine(State *state, int stepsToScoot){
    int moveDir = getBestMoveDirection_Line(state);

    if(moveDir == -1) {
        // No neighbors detected, search
        spinClockwise(60);
    } else if(moveDir == -2) {
        // In position, stop motors
        setMotorSteps(0, 0, 0);
    } else {
        // Move toward specified direction
        moveTowardsSensori(moveDir, stepsToScoot);
    }
}

void handlePolygon(State *state, int stepsToScoot){
    int moveDir = getBestMoveDirection_Polygon(state);

    if(moveDir == -1) {
        // No neighbors detected, search
        spinClockwise(60);
    } else if(moveDir == -2) {
        // In position, stop motors
        setMotorSteps(0, 0, 0);
    } else {
        // Move toward specified direction
        moveTowardsSensori(moveDir, stepsToScoot);
    }
}

//---------------------------------------------
// Main function call and FreeRTOS task
// Called in Task, Regularly updates the target steps for the motors to move towards
void handleMotors(State *state, int stepsToScoot) {
    switch (state->mode) {
        case State::OFF:
            setMotorSteps(0, 0, 0);
            break;
        case State::IDLE:
            handleIdle(state, stepsToScoot);
            break;
        case State::LINE:
            handleLine(state, stepsToScoot);
            break;
        case State::POLYGON:
            handlePolygon(state, stepsToScoot);
            break;
        case State::MANUAL:
            //No Action, Robot Only Moves In Response to Explicit Move Commands
            break;
        default:
            // Optional: handle unexpected state
            break;
    }

    moveMotors();
}

void motorTask(void* parameter) {
  const TickType_t xFrequency = pdMS_TO_TICKS(1); // Run every 1ms for smooth motion
  TickType_t xLastWakeTime = xTaskGetTickCount();

  // Get current state safely
  State localState;
  
  while (true) {

    //If SensorModule has the mutex, this means that it hasn't finished writing
    //the sensor data to the state, which means the motor should move according
    //to the last state
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      localState = state;
      xSemaphoreGive(stateMutex);
    }
    
    // Handle motor logic (doesn't need mutex, just reads state)
    handleMotors(&localState, 100);
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}