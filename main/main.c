
#include <stdio.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_err.h"

#include "comp_wifi.h"
#include "comp_mqtt.h"
#include "comp_i2c.h"
#include "comp_htu21.h"

static RTC_DATA_ATTR struct timeval sleep_enter_time;

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

void perform_startup_tasks()
{

}

int64_t difftime_us(struct timeval time1, struct timeval time2)
{
    return ((int64_t)time2.tv_sec - (int64_t)time1.tv_sec)*1000000L + ((int64_t)time2.tv_usec - (int64_t)time1.tv_usec);
}

void enter_deep_sleep(struct timeval wakeup_time)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    const int64_t config_sleep_duration_us = CONFIG_WAKEUP_PERIOD_MINUTES * 60 * 1000000L;

    int64_t sleep_duration_us = config_sleep_duration_us - difftime_us(wakeup_time, now);

    if(sleep_duration_us < 1000)
    {
        sleep_duration_us = 1000;
    }

    if(sleep_duration_us > config_sleep_duration_us)
    {
        sleep_duration_us = config_sleep_duration_us;
    }

    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(sleep_duration_us));
    ESP_LOGI(TAG, "Entering deep sleep for %ld microseconds", (long)sleep_duration_us);
    gettimeofday(&sleep_enter_time, NULL);
    esp_deep_sleep_start();
}

void app_main()
{
    struct timeval wakeup_time;
    gettimeofday(&wakeup_time, NULL);

    short fresh_boot = 1;
    long sleep_duration; 

    switch(esp_sleep_get_wakeup_cause())
    {
            case ESP_SLEEP_WAKEUP_TIMER:              
                sleep_duration = difftime_us(sleep_enter_time, wakeup_time);
                ESP_LOGI(TAG, "Wake-up caused by timer after %ld microseconds in deep sleep", sleep_duration);
                fresh_boot = 0;

                break;

            case ESP_SLEEP_WAKEUP_UNDEFINED:
                ESP_LOGI(TAG, "Wake-up cause undefined, this is a fresh boot");
                break;

            default:
                ESP_LOGW(TAG, "Wake-up caused by unexpected source");
    }

    vTaskDelay(2000/portTICK_PERIOD_MS);

    enter_deep_sleep(wakeup_time);
/*
    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(i2c_init());
    ESP_ERROR_CHECK(htu21_soft_reset(0));

    ESP_ERROR_CHECK(wifi_init());
    ESP_ERROR_CHECK(wifi_connect());

    ESP_ERROR_CHECK(mqtt_init());
    ESP_ERROR_CHECK(mqtt_start());




    while(1)
    {
        float temperature, humidity;

        ESP_ERROR_CHECK(htu21_get_temperature(&temperature));
        ESP_ERROR_CHECK(htu21_get_humidity(&humidity));
        humidity = htu21_get_compensated_humidity(humidity, temperature);

        ESP_LOGI(TAG, "Current temperature: %.2fC, Current relative humidity: %.1f%%", temperature, humidity);

        vTaskDelay(5000/portTICK_PERIOD_MS);

        
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
        
    }
    */
}
