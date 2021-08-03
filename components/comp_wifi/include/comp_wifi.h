#ifndef H_COMP_WIFI
#define H_COMP_WIFI

#include "esp_err.h"

esp_err_t wifi_init();
esp_err_t wifi_connect();
esp_err_t wifi_disconnect();
esp_err_t wifi_stop();
int wifi_is_connected();

#endif
