
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "comp_wifi.h"
#include "comp_mqtt.h"
#include "comp_i2c.h"
#include "comp_htu21.h"

static int mqtt_message_number = 0;

static const char* TAG = "Main Application Module";

void countdown_and_abort(unsigned int seconds)
{
    for( ; seconds > 0; --seconds)
    {
        ESP_LOGI(TAG, "Restarting in %d seconds...", seconds);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    abort();
}

void app_main()
{
    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
/*
    //initialize WiFi
    ESP_ERROR_CHECK(wifi_init());

    //connect to WiFi
    ESP_ERROR_CHECK(wifi_connect());

    //initialize MQTT
    ESP_ERROR_CHECK(mqtt_init());

    //start MQTT client
    ESP_ERROR_CHECK(mqtt_start());
*/

    ESP_ERROR_CHECK(i2c_init());

    ESP_ERROR_CHECK(htu21_soft_reset());

    while(1)
    {
        float temperature, humidity;

        ESP_ERROR_CHECK(htu21_get_temperature(&temperature));
        ESP_ERROR_CHECK(htu21_get_humidity(&humidity));
        humidity = htu21_get_compensated_humidity(humidity, temperature);

        ESP_LOGI(TAG, "Current temperature: %.2fC, Current relative humidity: %.1f%%", temperature, humidity);

        vTaskDelay(5000/portTICK_PERIOD_MS);

        /*
        char buffer[32];
        int length = sprintf(buffer, "Message %d", mqtt_message_number++);

        if(length <= 0)
        {
            ESP_LOGE(TAG, "Error creating MQTT message payload");
            countdown_and_abort(10);
        }.
        ++length;

        ESP_LOGI(TAG, "Sending MQTT message \"%s\"", buffer);

        ESP_ERROR_CHECK(mqtt_publish(CONFIG_MQTT_TOPIC, buffer, length, 1));

        vTaskDelay(10000 / portTICK_PERIOD_MS);
        */
    }
}
