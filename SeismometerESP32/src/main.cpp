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
#include "esp_mesh.h"

//#include "i2s_task.h"
//#include "wifi_task.h"
//#include "dsp_task.h"
#include "esp_attr.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
//#include "timer_group_struct.h"
#include "System.hpp"

LedTask System::Led(GPIO_LED_PIN);
WiFiTask System::WiFi;
StateMachine System::StateMch;
MessageQueue<I2S_To_DSP_Package_t> _i2s_to_dsp_queue(10);
MessageQueue<NetQueueElement_t> _dsp_to_mqtt_queue(10);


static const char *TAG = "MAIN";
//volatile bool wifi_connected = false;
// Define a spinlock to be used in critical sections

/*
static gptimer_handle_t hr_timer = NULL;

uint64_t high_resolution_timer_get( void ) {
    uint64_t val = 0;
    gptimer_get_raw_count(hr_timer, &val);
    return val;
}


uint64_t hrtim_to_tsf() {
    //uint64_t current_hrtim = high_resolution_timer_get();
    //return (uint64_t)(hrtim_gain * (double)current_hrtim + hrtim_offset);
}*/






/*
void high_resolution_timer_init(void) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 40000000,  // 40MHz → 1 tick = 25ns
    };

    gptimer_new_timer(&timer_config, &hr_timer);
    gptimer_enable(hr_timer);
    gptimer_start(hr_timer);
}*/

/*
void compute_linear_regression(const sync_frame_t *samples, int count, double *slope, double *intercept) {
    double sum_x = 0;
    double sum_y = 0;
    double sum_xy = 0;
    double sum_x2 = 0;
    
    for (int i = 0; i < count; i++) {
        // Cast to double for precision
        double x = (double)samples[i].hrtim;
        double y = (double)samples[i].tfs;
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double denominator = (count * sum_x2 - sum_x * sum_x);
    if (denominator == 0) {
        *slope = 0;
        *intercept = 0;
        return;
    }
    
    *slope = (count * sum_xy - sum_x * sum_y) / denominator;
    *intercept = (sum_y - (*slope) * sum_x) / count;
}

void tfs_sync_task(void *arg) {
    //init_timer();
    high_resolution_timer_init();
    ESP_LOGI(TAG, "Starting TSF sync task. Collecting %d samples over %d seconds.",
             TFS_SYNC_ARRAY_COUNT, TFS_SYNC_TIME_PERIOD_S);
    
    sync_frame_t samples[TFS_SYNC_ARRAY_COUNT];
    int sample_index = 0;
    
    while (1) {
        sync_frame_t new_frame;
        vTaskDelay(pdMS_TO_TICKS(100));
        
        new_frame.tfs = esp_mesh_get_tsf_time();
        if (new_frame.tfs == 0) {
            
        }
        printf("%llu\n", new_frame.tfs);
    }
}
*/



extern "C" void app_main( void ) {
    esp_log_level_set("*", ESP_LOG_INFO);
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    System::Start();

    //i2s_raw_adc_queue = xQueueCreate(I2S_RAW_QUEUE_LENGTH, sizeof(i2s_dma_package_t));
    //package_send_queue = xQueueCreate(TCP_PACKAGE_QUEUE_LENGTH, TCP_PACKAGE_LENGTH);

    // Initialize Wi-Fi
    //wifi_init_sta();
    //gpio_isr_init();
    //sound_pack_task
    //xTaskCreate(tcp_send_task, "TCP", 8000, NULL, 1, NULL);
    //xTaskCreate(tfs_sync_task, "TFS", 50000, NULL, configMAX_PRIORITIES-1, &tfs_sync_task_handler);
    //xTaskCreate(i2s_task, "I2S", 2048, NULL, 4, &i2s_task_handle);
    //xTaskCreate(dsp_task, "DSP", 2048, NULL, 3, &dsp_task_handle);
    //xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE + 100, NULL, 1, &blink_task_handle);

    //i2s_init();
}

void System::Start( void ){
    Led.Start("LED", 2000, tskIDLE_PRIORITY+1);
    StateMch.Start("State", 3000, tskIDLE_PRIORITY+2);
    WiFi.SetCredits(WIFI_SSID, WIFI_PWD);
    WiFi.Start("WiFi", 4000, tskIDLE_PRIORITY+1);
}
