#include <stdio.h>
#include "comp_i2c.h"
#include "esp_log.h"
#include "driver/i2c.h"

esp_err_t err;
#define RETURN_ON_ERROR(expr) if((err = (expr)) != ESP_OK) {return err;}

const i2c_port_t i2c_port = I2C_NUM_0;
const char* TAG = "I2C Module";
const int i2c_clock_speed = CONFIG_I2C_CLOCK_SPEED * 1000; //convert from kHz in menuconfig to Hz

esp_err_t i2c_init()
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_I2C_SDA_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = CONFIG_I2C_SCL_PIN,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_clock_speed
    };

    RETURN_ON_ERROR(i2c_param_config(i2c_port, &cfg));
    ESP_LOGI(TAG, "Configured parameters for port %d", (int)i2c_port);

    RETURN_ON_ERROR(i2c_driver_install(i2c_port, cfg.mode, 0, 0, 0));
    ESP_LOGI(TAG, "Installed driver for port %d", (int)i2c_port);

    return ESP_OK;
}
