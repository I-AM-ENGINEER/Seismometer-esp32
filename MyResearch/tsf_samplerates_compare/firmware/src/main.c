#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#define WIFI_SSID                           "TSF"
#define WIFI_PASS                           "12345678"

static const char *TAG = "wifi_app";
void tsf_task( void* arg );

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data){
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi started, connecting to AP...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Disconnected from AP, retrying...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Start TSF capturing");
            xTaskCreate(tsf_task, "TSF", 4000, NULL, configMAX_PRIORITIES - 1, NULL);
        }
    }
}

gptimer_handle_t high_resolution_timer_init( void ) {
    gptimer_handle_t hr_timer;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 40000000,  // 40MHz → 1 tick = 25ns
    };
    gptimer_new_timer(&timer_config, &hr_timer);
    gptimer_enable(hr_timer);
    gptimer_start(hr_timer);
    return hr_timer;
}

typedef struct {
    uint64_t timestamp_tsf;
    uint64_t timestamp_tim;  
} point_t;

gptimer_handle_t hr_timer;

void tsf_task( void* arg ){
    esp_log_level_set("*", ESP_LOG_NONE); // Disable log just for clean capture
    vTaskDelay(pdMS_TO_TICKS(5000));
    hr_timer = high_resolution_timer_init();

    printf("\n***START LOG***\n");
    TickType_t task_timestamp = xTaskGetTickCount();
    while(1){
        uint64_t timestamp_tsf, timestamp_tim;
        gptimer_get_raw_count(hr_timer, &timestamp_tim);
        timestamp_tsf = esp_mesh_get_tsf_time();
        printf("%llu\t%llu\n", timestamp_tim, timestamp_tsf);
        xTaskDelayUntil(&task_timestamp, pdMS_TO_TICKS(10));
    }
}

void app_main(void){
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}
