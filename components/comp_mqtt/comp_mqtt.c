#include <stdio.h>
#include "comp_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"

#define MQTT_BROKER_URL             CONFIG_MQTT_BROKER_URL
#define MQTT_USERNAME               CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD               CONFIG_MQTT_PASSWORD

#define MQTT_CONNECTED_BIT          BIT0

static const char* TAG = "MQTT Module";
static EventGroupHandle_t event_group_mqtt;
static esp_mqtt_client_handle_t mqtt_handle;

void on_mqtt_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case MQTT_EVENT_CONNECTED :
            ESP_LOGI(TAG, "Connected to broker");
            xEventGroupSetBits(event_group_mqtt, MQTT_CONNECTED_BIT);
            break;
    }
}

esp_err_t mqtt_init()
{
    //create event group MQTT
    event_group_mqtt = xEventGroupCreate();
    if(event_group_mqtt == NULL)
    {
        ESP_LOGE(TAG, "Error creating MQTT event group");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    //configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER_URL,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD
    };

    //initialize mqtt client
    mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    if(mqtt_handle == NULL)
    {
        ESP_LOGE(TAG, "Error initializing MQTT client");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    ESP_LOGI(TAG, "Client initialized");

    //register MQTT event handler
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_handle, ESP_EVENT_ANY_ID, on_mqtt_event, NULL));

    return ESP_OK;
}

esp_err_t mqtt_start()
{
    //start MQTT client
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_handle));
    ESP_LOGI(TAG, "Client started");

    //wait for MQTT connection to broker
    xEventGroupWaitBits(
        event_group_mqtt,
        MQTT_CONNECTED_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    return ESP_OK;
}

esp_err_t mqtt_publish(const char* topic, const char* message, int length, int qos)
{
    esp_mqtt_client_publish(
        mqtt_handle,
        topic,
        message,
        length,
        qos,
        0
    );

    return ESP_OK;
}
