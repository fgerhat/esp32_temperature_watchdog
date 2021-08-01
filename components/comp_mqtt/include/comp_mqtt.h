#ifndef H_COMP_MQTT
#define H_COMP_MQTT

#include "esp_err.h"

esp_err_t mqtt_init();
esp_err_t mqtt_start();
esp_err_t mqtt_publish(const char* topic, const char* message, int length, int qos);

#endif