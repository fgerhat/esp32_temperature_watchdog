#include <stdio.h>
#include "comp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define WIFI_SSID                   CONFIG_WIFI_SSID
#define WIFI_PASSWORD               CONFIG_WIFI_PASSWORD
#define WIFI_MAX_RETRY_ATTEMPTS     CONFIG_WIFI_MAX_RETRY_ATTEMPTS

#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

esp_err_t err;
#define RETURN_ON_ERROR(expr) if((err = (expr)) != ESP_OK) {return err;}

typedef enum wifi_state
{
    WIFI_CONNECTED_STATE,
    WIFI_DISCONNECTED_STATE
} WifiState;

static const char* TAG = "WiFi Module";
static EventGroupHandle_t event_group_wifi;
static int wifi_retries = 0;
static WifiState wifi_desired_state = WIFI_DISCONNECTED_STATE;
static WifiState wifi_actual_state = WIFI_DISCONNECTED_STATE;

void on_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "Connecting...");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            wifi_actual_state = WIFI_DISCONNECTED_STATE;

            if(wifi_desired_state == WIFI_DISCONNECTED_STATE)
            {
                ESP_LOGI(TAG, "Successfully disconected");
                break;
            }

            if(wifi_retries < WIFI_MAX_RETRY_ATTEMPTS)
            {
                ESP_LOGW(TAG, "WiFi connection failed, retrying (attempt %d)...", ++wifi_retries);
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(TAG, "Could not connect to WiFi!");
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
            wifi_actual_state = WIFI_CONNECTED_STATE;

            wifi_retries = 0;
            ESP_LOGI(TAG, "Connected");
            xEventGroupSetBits(event_group_wifi, WIFI_CONNECTED_BIT);
            break;
    }
}


esp_err_t wifi_init()
{
    //create event group wifi
    event_group_wifi = xEventGroupCreate();
    if(event_group_wifi == NULL)
    {
        ESP_LOGE(TAG, "Error creating WiFi event group");
        return ESP_FAIL;
    }

    //initialize NETIF
    RETURN_ON_ERROR(esp_netif_init());
    //create default event loop
    RETURN_ON_ERROR(esp_event_loop_create_default());
    //create default wifi station
    esp_netif_create_default_wifi_sta();
    //initialize default WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    RETURN_ON_ERROR(esp_wifi_init(&cfg));

    //wifi configuration
    RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t conf = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD   
        }
    };
    RETURN_ON_ERROR(esp_wifi_set_config(ESP_IF_WIFI_STA, &conf));

    //register wifi and ip event handlers
    esp_event_handler_instance_t instance_wifi_handler;
    esp_event_handler_instance_t instance_ip_handler;
    RETURN_ON_ERROR(esp_event_handler_instance_register(
        WIFI_EVENT, 
        ESP_EVENT_ANY_ID, 
        &on_wifi_event, 
        NULL, 
        &instance_wifi_handler));
    RETURN_ON_ERROR(esp_event_handler_instance_register(
        IP_EVENT, 
        ESP_EVENT_ANY_ID, 
        &on_ip_event, 
        NULL, 
        &instance_ip_handler));

    return ESP_OK;
}

esp_err_t wifi_connect()
{
    wifi_desired_state = WIFI_CONNECTED_STATE;

    //start wifi driver
    RETURN_ON_ERROR(esp_wifi_start());

    //wait for wifi successful connection or failure
    EventBits_t event_bits_wifi = xEventGroupWaitBits(
        event_group_wifi,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY);

    if(event_bits_wifi & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "No WiFi connection");
        wifi_desired_state = WIFI_DISCONNECTED_STATE;
        return ESP_FAIL;
    }
    else if(event_bits_wifi & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to WiFi");
    }
    else
    {
        ESP_LOGE(TAG, "Unknown error related to WiFi module event bits");
        wifi_desired_state = WIFI_DISCONNECTED_STATE;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t wifi_disconnect()
{
    wifi_desired_state = WIFI_DISCONNECTED_STATE;

    RETURN_ON_ERROR(esp_wifi_disconnect());

    return ESP_OK;
}

esp_err_t wifi_stop()
{
    if(wifi_desired_state != WIFI_DISCONNECTED_STATE)
    {
        RETURN_ON_ERROR(wifi_disconnect());
    }

    RETURN_ON_ERROR(wifi_stop());

    return ESP_OK;
}

int wifi_is_connected()
{
    return (wifi_actual_state == WIFI_CONNECTED_STATE) ? 1 : 0;
}
