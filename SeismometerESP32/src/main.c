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
#include "driver/timer.h"
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

/*
static void inline print_timer_counter(uint64_t counter_value){
    printf("Counter: 0x%08x%08x\r\n", (uint32_t) (counter_value >> 32),
           (uint32_t) (counter_value));
    printf("Time   : %.8f s\r\n", (double) counter_value / TIMER_SCALE);
}*/

void high_resolution_timer_init( void ){
    timer_config_t config = {
        .divider = 2,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_DIS,
        .auto_reload = TIMER_AUTORELOAD_DIS,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

inline uint64_t high_resolution_timer_get( void ){
    uint64_t val;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &val);
    return val;
}

#define TFS_SYNC_TIME_PERIOD_S      (5)
#define TFS_SYNC_TFS_PERIOD_MS      (100)
#define TFS_SYNC_ARRAY_COUNT        ((1000/TFS_SYNC_TFS_PERIOD_MS)*TFS_SYNC_TIME_PERIOD_S)
#define CALIB_BUFFER_SIZE 60

typedef struct{
    uint64_t tfs;
    uint64_t hrtim;
}sync_frame_t;

volatile float hrtim_gain;
volatile float hrtim_offset;

static void compute_calibration(const sync_frame_t *frames, uint32_t n, float *gain, float *offset)
{
    if (n < 2) {
        ESP_LOGW("TFS_SYNC", "Not enough samples for calibration.");
        return;
    }

    float sum_gain = 0.0f;
    uint32_t valid_pairs = 0;

    for (uint32_t i = 0; i < n - 1; i++) {
        uint64_t delta_tfs = frames[i+1].tfs - frames[i].tfs;
        uint64_t delta_hrtim = frames[i+1].hrtim - frames[i].hrtim;
        if (delta_hrtim == 0) {
            continue;
        }
        float local_gain = ((float)delta_tfs) / ((float)delta_hrtim);
        sum_gain += local_gain;
        valid_pairs++;
    }

    if (valid_pairs == 0) {
        ESP_LOGW("TFS_SYNC", "No valid adjacent pairs for calibration.");
        return;
    }

    float avg_gain = sum_gain / valid_pairs;

    // Compute the offset: average over all samples of (tfs - avg_gain * hrtim)
    float sum_offset = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float local_offset = (float)frames[i].tfs - avg_gain * (float)frames[i].hrtim;
        sum_offset += local_offset;
    }
    float avg_offset = sum_offset / n;

    *gain = avg_gain;
    *offset = avg_offset;
}


void tfs_sync_task( void *arg ){

    static float calib_gain_buffer[CALIB_BUFFER_SIZE] = {0};
    static float calib_offset_buffer[CALIB_BUFFER_SIZE] = {0};
    static uint32_t calib_buffer_index = 0;
    static uint32_t calib_buffer_count = 0; 

    high_resolution_timer_init();
    uint16_t sync_stream_pos = 0;
    sync_frame_t sync_arr[TFS_SYNC_ARRAY_COUNT];
    ESP_LOGI(TAG, "Starting TSF sync task. Collecting %d samples over %d seconds.",
             TFS_SYNC_ARRAY_COUNT, TFS_SYNC_TIME_PERIOD_S);
    while (1){
        sync_frame_t new_frame;
        new_frame.hrtim = high_resolution_timer_get();
        new_frame.tfs = esp_wifi_get_tsf_time(ESP_IF_WIFI_STA);
        if(new_frame.tfs == 0){
            sync_stream_pos = 0;
            ESP_LOGE(TAG, "Unable to catch TFS");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        sync_arr[sync_stream_pos++] = new_frame;

        if (sync_stream_pos >= TFS_SYNC_ARRAY_COUNT) {
            float new_gain, new_offset;
            compute_calibration(sync_arr, TFS_SYNC_ARRAY_COUNT, &new_gain, &new_offset);

            float effective_gain = (new_gain != 0.0f) ? (1.0f / new_gain) : 40.0f;

            // Insert the new calibration into the ring buffer.
            calib_gain_buffer[calib_buffer_index]   = effective_gain;
            calib_offset_buffer[calib_buffer_index] = new_offset;
            calib_buffer_index = (calib_buffer_index + 1) % CALIB_BUFFER_SIZE;
            if (calib_buffer_count < CALIB_BUFFER_SIZE) {
                calib_buffer_count++;
            }

            // Compute the moving average of gain and offset over the last calib_buffer_count calibrations.
            float sum_gain = 0.0f;
            float sum_offset = 0.0f;
            for (uint32_t i = 0; i < calib_buffer_count; i++) {
                sum_gain   += calib_gain_buffer[i];
                sum_offset += calib_offset_buffer[i];
            }
            float avg_gain = sum_gain / calib_buffer_count;
            float avg_offset = sum_offset / calib_buffer_count;

            // Update the global calibration parameters.
            hrtim_gain   = avg_gain;
            hrtim_offset = avg_offset;
            ESP_LOGI(TAG, "Calibration updated: hrtim_gain = %.6f, hrtim_offset = %.6f (moving average over %lu calibrations)", hrtim_gain, hrtim_offset, calib_buffer_count);

            // Reset the sample index for the next 5-second window.
            sync_stream_pos = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(TFS_SYNC_TFS_PERIOD_MS));
    }
}

        //printf("%llu %llu\n", tsf_time, time1, time2);

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
    xTaskCreate(tfs_sync_task, "TFS", 50000, NULL, 2, NULL);
    //xTaskCreate(i2s_task, "I2S", 2048, NULL, 4, &i2s_task_handle);
    //xTaskCreate(dsp_task, "DSP", 2048, NULL, 3, &dsp_task_handle);
    //xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE + 100, NULL, 1, &blink_task_handle);

    //i2s_init();
}
