
#include <stdio.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_err.h"
#include "esp_sntp.h"

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

short app_init_i2c_htu()
{
    esp_err_t ret;

    ret = i2c_init();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing I2C");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ret = htu21_soft_reset(1);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error sending soft reset command to HTU21");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ESP_LOGI(TAG, "HTU21 sensor OK");
    return 1;
}

short app_init_wifi()
{
    esp_err_t ret;

    ret = wifi_init();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing wifi");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ret = wifi_connect();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error connecting to wifi");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ESP_LOGI(TAG, "Wifi OK");
    return 1;
}

short app_init_mqtt()
{
    esp_err_t ret;

    ret = mqtt_init();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing MQTT");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ret = mqtt_start();
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error starting MQTT client");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ESP_LOGI(TAG, "MQTT OK");
    return 1;
}

short app_measure_temp_humidity(float* temp, float*rh)
{
    esp_err_t ret;

    ret = htu21_get_temperature(temp);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading temperature from HTU21");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }

    ret = htu21_get_humidity(rh);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading humidity from HTU21");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        return 0;
    }
    *rh = htu21_get_compensated_humidity(*rh, *temp);

    return 1;
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

    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing NVS");
        ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
        enter_deep_sleep(wakeup_time);
    }
    ESP_LOGI(TAG, "NVS OK");

    short htu_ok = app_init_i2c_htu();
    short wifi_ok = app_init_wifi();
    short mqtt_ok = 0; 
    if(wifi_ok)
    {
        /*sntp_setservername(0, "pool.ntp.org");
        sntp_init();
        struct timeval time_sntp;
        sntp_sync_time(&time_sntp);*/

        mqtt_ok = app_init_mqtt();
    }

    if(htu_ok)
    {
        float temperature, humidity;
        if(app_measure_temp_humidity(&temperature, &humidity))
        {
            ESP_LOGI(TAG, "Current temperature: %.2fC, Current RH: %.1f%%", temperature, humidity);
            if(mqtt_ok)
            {
                char buffer1[32];
                char buffer2[32];
                char buffer3[32];
                int length;
                
                length = sprintf(buffer2, "%.1f", humidity);
                if(length > 0)
                {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_publish("filip.gerhat.1@gmail.com/esp32/humidity", buffer2, length, 1));
                }
                else
                {
                    ESP_LOGE(TAG, "Error writing to buffer for humidity message");
                }

                //vTaskDelay(1000/portTICK_PERIOD_MS);

                length = sprintf(buffer1, "%.1f", temperature);
                if(length > 0)
                {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_publish("filip.gerhat.1@gmail.com/esp32/temperature", buffer1, length, 1));
                }
                else
                {
                    ESP_LOGE(TAG, "Error writing to buffer for temperature message");
                }

                //vTaskDelay(1000/portTICK_PERIOD_MS);

                length = sprintf(buffer3, "OK");
                if(length > 0)
                {
                    ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_publish("filip.gerhat.1@gmail.com/esp32/diag", buffer3, length, 1));
                }               
                else
                {
                    ESP_LOGE(TAG, "Error writing to buffer for diag OK message");
                }
            }
        }
    }

    if(mqtt_ok && (!htu_ok))
    {
        char buffer[32];

        int length = sprintf(buffer, "HTU ERR");
        ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_publish("filip.gerhat.1@gmail.com/esp32/diag", buffer, length, 1));
    }

    //ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_disconnect());
    //ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_stop());

    vTaskDelay(5000/portTICK_PERIOD_MS);
    enter_deep_sleep(wakeup_time);





    /*while(1)
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
        
    }*/
}
