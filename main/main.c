
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#define WIFI_SSID                   CONFIG_WIFI_SSID
#define WIFI_PASSWORD               CONFIG_WIFI_PASSWORD
#define WIFI_MAX_RETRY_ATTEMPTS     CONFIG_WIFI_MAX_RETRY_ATTEMPTS

#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

#define MQTT_BROKER_URL             CONFIG_MQTT_BROKER_URL
#define MQTT_USERNAME               CONFIG_MQTT_USERNAME
#define MQTT_PASSWORD               CONFIG_MQTT_PASSWORD
#define MQTT_TOPIC                  CONFIG_MQTT_TOPIC

#define MQTT_CONNECTED_BIT          BIT0

static int wifi_retries = 0;
static int mqtt_message_number = 0;

static EventGroupHandle_t event_group_wifi;
static EventGroupHandle_t event_group_mqtt;

static const char* TAG_WIFI = "WiFi Module";
static const char* TAG_APP = "Main Application Module";
static const char* TAG_MQTT = "MQTT Module";

void on_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG_WIFI, "Connecting...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if(wifi_retries < WIFI_MAX_RETRY_ATTEMPTS)
            {
                ESP_LOGW(TAG_WIFI, "WiFi connection failed, retrying (attempt %d)...", ++wifi_retries);
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(TAG_WIFI, "Could not connect to WiFi!");
                xEventGroupSetBits(event_group_wifi, WIFI_FAIL_BIT);
            }
            break;
    }
}

void on_ip_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case IP_EVENT_STA_GOT_IP:
            wifi_retries = 0;
            ESP_LOGI(TAG_WIFI, "Connected");
            xEventGroupSetBits(event_group_wifi, WIFI_CONNECTED_BIT);
            break;
    }
}

void on_mqtt_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case MQTT_EVENT_CONNECTED :
            ESP_LOGI(TAG_MQTT, "Connected to broker");
            xEventGroupSetBits(event_group_mqtt, MQTT_CONNECTED_BIT);
            break;
    }
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

    //create event group wifi
    event_group_wifi = xEventGroupCreate();
    if(event_group_wifi == NULL)
    {
        ESP_LOGE(TAG_APP, "Error creating WiFi event group");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    //initialize NETIF
    ESP_ERROR_CHECK(esp_netif_init());
    //create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //create default wifi station
    esp_netif_create_default_wifi_sta();
    //initialize default WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //wifi configuration
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t conf = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD   
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &conf));

    //register wifi and ip event handlers
    esp_event_handler_instance_t instance_wifi_handler;
    esp_event_handler_instance_t instance_ip_handler;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, 
        ESP_EVENT_ANY_ID, 
        &on_wifi_event, 
        NULL, 
        &instance_wifi_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, 
        ESP_EVENT_ANY_ID, 
        &on_ip_event, 
        NULL, 
        &instance_ip_handler));

    //start wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    //wait for wifi successful connection or failure
    EventBits_t event_bits_wifi = xEventGroupWaitBits(
        event_group_wifi,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    if(event_bits_wifi & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG_APP, "No WiFi connection");

        for (int i = 10; i >= 0; i--) {
            printf("Restarting in %d seconds...\n", i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        printf("Restarting now.\n");
        fflush(stdout);
        esp_restart();
    }
    else if(event_bits_wifi & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG_APP, "Connected to WiFi");
    }

    //create event group MQTT
    event_group_mqtt = xEventGroupCreate();
    if(event_group_mqtt == NULL)
    {
        ESP_LOGE(TAG_APP, "Error creating MQTT event group");
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
    esp_mqtt_client_handle_t mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    if(mqtt_handle == NULL)
    {
        ESP_LOGE(TAG_MQTT, "Error initializing MQTT client");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    ESP_LOGI(TAG_MQTT, "Client initialized");

    //register MQTT event handler
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_handle, ESP_EVENT_ANY_ID, on_mqtt_event, NULL));

    //start MQTT client
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_handle));
    ESP_LOGI(TAG_MQTT, "Client started");

    //wait for MQTT connection to broker
    xEventGroupWaitBits(
        event_group_mqtt,
        MQTT_CONNECTED_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    while(1)
    {
        char buffer[32];
        int length = sprintf(buffer, "Message %d", mqtt_message_number++);

        if(length <= 0)
        {
            ESP_LOGE(TAG_APP, "Error creating MQTT message payload");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            abort();
        }
        ++length;

        ESP_LOGI(TAG_APP, "Sending MQTT message \"%s\"", buffer);

        esp_mqtt_client_publish(
            mqtt_handle,
            MQTT_TOPIC,
            buffer,
            length,
            1,
            0
        );

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    printf("Hello world!\n");
}
