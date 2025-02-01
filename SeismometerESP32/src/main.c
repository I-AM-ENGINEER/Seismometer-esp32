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
#include "driver/gpio.h"
//#include "driver/i2s.h"
#include "esp_private/i2s_platform.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
//#include "driver/periph_ctrl.h"
#include "soc/soc.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "config.h"
//#include ""

#include "i2s_task.h"
#include "wifi_task.h"
#include "dsp_task.h"

static const char *TAG = "MAIN";
TaskHandle_t i2s_task_handle = NULL;
TaskHandle_t blink_task_handle = NULL;
TaskHandle_t dsp_task_handle = NULL;
TaskHandle_t mqtt_task_handler = NULL;
static volatile int64_t tsf_timestamp = 0;
volatile bool wifi_connected = false;
// Define a spinlock to be used in critical sections

QueueHandle_t i2s_raw_adc_queue = NULL;
QueueHandle_t package_send_queue = NULL;

void blink_task( void *arg ) {
    gpio_reset_pin(GPIO_LED_PIN);
    gpio_set_direction(GPIO_LED_PIN, GPIO_MODE_OUTPUT);
    while (true) {
        gpio_set_level(GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void button_task( void *arg ){
    // Configure GPIO input and interrupt
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,         // Trigger on falling edge
        .mode = GPIO_MODE_INPUT,                // Set as input
        .pin_bit_mask = (1ULL << GPIO_BTN_PIN), // GPIO pin mask
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // No pull-down
        .pull_up_en = GPIO_PULLUP_ENABLE        // Enable pull-up
    };
    gpio_config(&io_conf);

    while (1){
        
    }
}

void app_main( void ) {
    esp_log_level_set("*", ESP_LOG_INFO);
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    i2s_raw_adc_queue = xQueueCreate(I2S_RAW_QUEUE_LENGTH, sizeof(i2s_dma_package_t));
    package_send_queue = xQueueCreate(TCP_PACKAGE_QUEUE_LENGTH, TCP_PACKAGE_LENGTH);

    // Initialize Wi-Fi
    wifi_init_sta();
    //sound_pack_task
    //xTaskCreate(tcp_send_task, "TCP", 8000, NULL, 1, NULL);
    xTaskCreate(i2s_task, "I2S", 2048, NULL, 4, &i2s_task_handle);
    //xTaskCreate(dsp_task, "DSP", 2048, NULL, 3, &dsp_task_handle);
    //xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE + 100, NULL, 1, &blink_task_handle);

    //i2s_init();
}
