
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define WIFI_SSID                   CONFIG_WIFI_SSID
#define WIFI_PASSWORD               CONFIG_WIFI_PASSWORD
#define WIFI_MAX_RETRY_ATTEMPTS     CONFIG_WIFI_MAX_RETRY_ATTEMPTS

static int wifi_retries = 0;

void on_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    switch(id)
    {
        case WIFI_EVENT_STA_START:
            printf("wiFi connecting...\n");
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if(wifi_retries < WIFI_MAX_RETRY_ATTEMPTS)
            {
                printf("WiFi connection failed, retrying (attempt %d)...\n", ++wifi_retries);
                esp_wifi_connect();
            }
            //TODO improve, maybe use event groups or semaphores
            else
            {
                printf("Could not connect to WiFi!\n");
                fflush(stdout);
                esp_restart();
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
            printf("WiFi Connected!\n");
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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, &instance_wifi_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL, &instance_ip_handler));

    //start wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());



    printf("Hello world!\n");

    printf("Restarting in 1 minute\n");
    vTaskDelay(50000 / portTICK_PERIOD_MS);

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
