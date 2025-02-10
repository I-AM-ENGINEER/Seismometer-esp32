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

LedTask System::Led(GPIO_LED_PIN);
WiFiTask System::WiFi;
I2S_Task System::I2S_Reader;
StateMachine System::StateMch;
PrecisionTimeTask System::SyncTask;
MessageQueue<I2S_to_DSP_package_t> System::i2s_to_dsp_queue(1);
MessageQueue<NetQueueElement_t> System::dsp_to_mqtt_queue(10);

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
    Led.Start("LED", 2000, tskIDLE_PRIORITY+1);
    StateMch.Start("State", 3000, tskIDLE_PRIORITY+2);
    WiFi.SetCredits(WIFI_SSID, WIFI_PWD);
    WiFi.Start("WiFi", 4000, tskIDLE_PRIORITY+1);
    SyncTask.Start("TSF sync", 4000, configMAX_PRIORITIES - 1);
    I2S_Reader.Start("I2S", 5000, 10);
}
