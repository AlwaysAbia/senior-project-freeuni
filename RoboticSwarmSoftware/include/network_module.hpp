#ifndef NETWORK_MODULE_HPP
#define NETWORK_MODULE_HPP

void connectToHub();
void setupOTA();

void setupServer();

//FreeRTOS Task
void networkTask(void* parameter);

#endif