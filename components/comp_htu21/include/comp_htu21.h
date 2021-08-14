#ifndef H_COMP_HTU21
#define H_COMP_HTU21

#include "esp_err.h"

#define HTU21_ADDRESS 0x40

esp_err_t htu21_soft_reset(short wait_for_reset);
esp_err_t htu21_get_temperature(float* temperature);
esp_err_t htu21_get_humidity(float* humidity);
float htu21_get_compensated_humidity(float humidity, float temperature);

#endif