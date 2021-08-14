#include <stdio.h>
#include "comp_htu21.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"

esp_err_t err;
#define RETURN_ON_ERROR(expr) if((err = (expr)) != ESP_OK) {return err;}

//command definitions
#define CMD_SOFT_RESET 0xFE
#define CMD_MEASURE_TEMP_HOLD 0xE3
#define CMD_MEASURE_TEMP_NOHOLD 0xF3
#define CMD_MEASURE_HUM_HOLD 0xE5
#define CMD_MEASURE_HUM_NOHOLD 0xF5
#define CMD_WRITE_USER_REG 0xE6
#define CMD_READ_USER_REG 0xE7

static const i2c_port_t i2c_port = I2C_NUM_0;
static const char* TAG = "HTU21 Module";

// perform a CRC check of the message
// message should contain its CRC in the 8 least significant bits
// return 0 if the message is valid
uint32_t crc_check(uint32_t message)
{
    const uint32_t crc_polynomial = 0b100110001;

    uint32_t divisor = crc_polynomial << 15;

    for(int bit = 23; bit > 7; --bit)
    {
        if(message & (1<<bit))
        {
            message ^= divisor;
        }
        divisor >>= 1;
    }
    return message;
}

esp_err_t htu21_send_command(uint8_t command)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    RETURN_ON_ERROR(i2c_master_start(cmd));                                                     // START bit
    RETURN_ON_ERROR(i2c_master_write_byte(cmd, (HTU21_ADDRESS << 1) | I2C_MASTER_WRITE, 1));    // I2C address + write bit
    RETURN_ON_ERROR(i2c_master_write_byte(cmd, command, 1));                                    // command byte
    RETURN_ON_ERROR(i2c_master_stop(cmd));                                                      // STOP bit

    RETURN_ON_ERROR(i2c_master_cmd_begin(i2c_port, cmd, 1000/portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

esp_err_t htu21_read_data(uint8_t* data_buf)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    RETURN_ON_ERROR(i2c_master_start(cmd));                                                     // START bit
    RETURN_ON_ERROR(i2c_master_write_byte(cmd, (HTU21_ADDRESS << 1) | I2C_MASTER_READ, 1));     // I2C address + read bit
    RETURN_ON_ERROR(i2c_master_read(cmd, data_buf, 3, I2C_MASTER_LAST_NACK));                   // read 3 bytes of data
    RETURN_ON_ERROR(i2c_master_stop(cmd));                                                      // STOP bit

    RETURN_ON_ERROR(i2c_master_cmd_begin(i2c_port, cmd, 1000/portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

esp_err_t htu21_soft_reset(short wait_for_reset)
{
    const int reset_time_ms = 15;

    RETURN_ON_ERROR(htu21_send_command(CMD_SOFT_RESET));    
    ESP_LOGI(TAG, "Sent soft reset command");

    if(wait_for_reset)
    {
        vTaskDelay(reset_time_ms/portTICK_PERIOD_MS); //HTU21 soft reset should take no more than 15ms
    }
    
    return ESP_OK;
}

esp_err_t htu21_get_temperature(float* temperature)
{
    const int measurement_time_ms = 50;

    RETURN_ON_ERROR(htu21_send_command(CMD_MEASURE_TEMP_NOHOLD));

    // wait for measurement to finish
    vTaskDelay(measurement_time_ms/portTICK_PERIOD_MS);

    uint8_t data_buf[3];
    RETURN_ON_ERROR(htu21_read_data(data_buf));
    ESP_LOGD(TAG, "Successfully read temperature data");

    uint32_t data = (data_buf[0] << 16) | (data_buf[1] << 8) | data_buf[2];

    //CRC check
    uint32_t crc_result = crc_check(data);
    if(crc_result)
    {
        ESP_LOGE(TAG, "CRC check failed (0x%X)", crc_result);
        return ESP_FAIL;
    }

    uint16_t signal_temp = (data >> 8) & (~0b11);
    uint8_t status_bits = (data >> 8) & 0b11;

    // bit 1 of status bits should be 0 for temperature measurement
    if(status_bits & 0b10)
    {
        ESP_LOGE(TAG, "HTU returned humidity instead of temperature");
        return ESP_FAIL;
    }

    *temperature = -46.85 + 175.72*signal_temp/65536;

    return ESP_OK;
}

esp_err_t htu21_get_humidity(float* humidity)
{
    const int measurement_time_ms = 20;

    RETURN_ON_ERROR(htu21_send_command(CMD_MEASURE_HUM_NOHOLD));

    // wait for measurement to finish
    vTaskDelay(measurement_time_ms/portTICK_PERIOD_MS);

    uint8_t data_buf[3];
    RETURN_ON_ERROR(htu21_read_data(data_buf));
    ESP_LOGD(TAG, "Successfully read humidity data");

    uint32_t data = (data_buf[0] << 16) | (data_buf[1] << 8) | data_buf[2];

    //CRC check
    uint32_t crc_result = crc_check(data);
    if(crc_result)
    {
        ESP_LOGE(TAG, "CRC check failed (0x%X)", crc_result);
        return ESP_FAIL;
    }

    uint16_t signal_humidity = (data >> 8) & (~0b11);
    uint8_t status_bits = (data >> 8) & 0b11;

    // bit 1 of status bits should be 1 for humidity measurement
    if(!(status_bits & 0b10))
    {
        ESP_LOGE(TAG, "HTU returned temperature instead of humidity");
        return ESP_FAIL;
    }

    *humidity = -6 + 125*(float)signal_humidity/65536;

    return ESP_OK;
}

float htu21_get_compensated_humidity(float humidity, float temperature)
{
    const float temperature_coefficient = -0.15;
    return humidity + (25.0 - temperature) * temperature_coefficient;
}
