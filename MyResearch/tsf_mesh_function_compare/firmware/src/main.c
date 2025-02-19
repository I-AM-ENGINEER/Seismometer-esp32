#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"

#define USE_MESH_TSF_INSTEAD_WIFI           (true)

#define WIFI_SSID                           "5G_COVID_BASE_STATION"
#define WIFI_PASS                           "1SimplePass9"

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

void tsf_task( void* arg ){
    esp_log_level_set("*", ESP_LOG_NONE); // Disable log just for clean capture
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("\n***START LOG***\n");
    gptimer_handle_t hr_timer = high_resolution_timer_init();
    TickType_t task_timestamp = xTaskGetTickCount();
    while(1){
        uint64_t timestamp_tsf, timestamp_hrtim_tfs_start, timestamp_hrtim_tfs_end;
        gptimer_get_raw_count(hr_timer, &timestamp_hrtim_tfs_start);
        #if(USE_MESH_TSF_INSTEAD_WIFI)
        timestamp_tsf = esp_mesh_get_tsf_time();
        #else
        timestamp_tsf = esp_wifi_get_tsf_time(WIFI_IF_STA);
        #endif
        gptimer_get_raw_count(hr_timer, &timestamp_hrtim_tfs_end);
        printf("%llu\t%llu\t%llu\n",
            timestamp_hrtim_tfs_start, 
            timestamp_tsf,
            timestamp_hrtim_tfs_end
        );
        xTaskDelayUntil(&task_timestamp, pdMS_TO_TICKS(100));
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
