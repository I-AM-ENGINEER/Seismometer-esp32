#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "System.hpp"
#include "esp_task_wdt.h"

LedTask System::Led(GPIO_LED_PIN);
WiFiTask System::WiFi;
I2S_Task System::I2S_Reader;
StateMachine System::StateMch;
PrecisionTimeTask System::SyncTask;
DSP_Task System::DSP;
MQTT_Task System::MQTT;
BTN_CheckerTask System::BTN;
MessageQueue<I2S_to_DSP_package_t> System::i2s_to_dsp_queue(5);
MessageQueue<NetQueueElement_t> System::dsp_to_mqtt_queue(3);

static const char *TAG = "MAIN";

extern "C" void app_main( void ) {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    ESP_ERROR_CHECK(ret);
    System::Start();
}

void System::Start( void ){
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 30000,          // 10 seconds timeout
        .idle_core_mask = (1 << configNUM_CORES) - 1,
        .trigger_panic = true         // Trigger a panic if timeout occurs
    };
    esp_task_wdt_deinit();
    //esp_task_wdt_init(&wdt_config);
    //esp_task_wdt_add(NULL);
    // Initialize the Task Watchdog Timer with the given configuration
    //esp_task_wdt_init(&wdt_config);
    //esp_task_wdt_reconfigure(&wdt_config);
    // You can add tasks to be monitored by the TWDT if needed
    //esp_task_wdt_add(NULL);  // Add the current task (NULL refers to the current task)

    Led.Start("LED", 2000, tskIDLE_PRIORITY+2, NULL);
    StateMch.Start("State", 3000, tskIDLE_PRIORITY+3, NULL);
    WiFi.SetCredits(WIFI_SSID, WIFI_PWD);
    WiFi.Start("WiFi", 4000, tskIDLE_PRIORITY+2, NULL);
    SyncTask.Start("TSF sync", 4000, configMAX_PRIORITIES - 1, NULL);
    I2S_Reader.Start("I2S", 5000, 10, NULL);
    DSP.Start("DSP", 2000, 4, NULL);
    BTN.Start("BTN", 2000, 2, NULL);
    vTaskDelay(pdMS_TO_TICKS(5000));
    MQTT.Start("MQTT", 3000, 5, NULL);
}
