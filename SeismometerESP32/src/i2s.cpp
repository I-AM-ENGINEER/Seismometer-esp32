//#include "i2s_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_intr_alloc.h"
#include "soc/soc.h"
#include "driver/i2s_std.h"
#include "esp_wifi.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include "esp_intr_alloc.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/interrupts.h"
#include "I2S_Task.hpp"
#include "System.hpp"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "I2S";

#define TOGGLE_PIN GPIO_NUM_8   // Output pin to toggle
volatile bool toggle_state = false;
gptimer_handle_t i2s_timer = nullptr;

void setup_toggle_pin() {
    // Configure GPIO 8 as output
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << TOGGLE_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(TOGGLE_PIN, toggle_state); // Start with LOW state
}

void I2S_Task::Run( void* arg ) {
    setup_toggle_pin();
    i2s_event_callbacks_t callbacks = {
        .on_recv            = i2s_receive_ISR,
        .on_recv_q_ovf      = NULL,
        .on_sent            = NULL,
        .on_send_q_ovf      = NULL,
    };
    uint8_t* i2s_adc_raw_buffer = (uint8_t*)pvPortMalloc(I2S_DMA_BUFFER_SIZE);
    if(i2s_adc_raw_buffer == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }

    gpio_set_direction(GPIO_GAIN_1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_10_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_100_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_1000_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_GAIN_1_PIN, 1);
    gpio_set_level(GPIO_GAIN_10_PIN, 0);
    gpio_set_level(GPIO_GAIN_100_PIN, 0);
    gpio_set_level(GPIO_GAIN_1000_PIN, 0);

    volatile uint64_t timestamp;
    struct i2s_isr_arg isr_arg = {
        .task = _handle,
        .timestamp = (uint64_t*)&timestamp
    };

    ESP_LOGI(TAG, "I2S initializing...");
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &i2s_rx_config));
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_chan, &callbacks, (void*)&isr_arg));
    
    ESP_ERROR_CHECK(gptimer_new_timer(&i2s_tim_cfg, &i2s_timer));
    ESP_ERROR_CHECK(gptimer_enable(i2s_timer));
    ESP_ERROR_CHECK(gptimer_start(i2s_timer));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    I2S_to_DSP_package_t package = {
        .data = nullptr,
        .samples_count = I2S_SAMPLES_MONO_FRAME/2,
        .timestamp = 0,
        .channels_count = 1,
        .bits_per_single_sample = 16
    };
    while (1) {
        i2s_channel_read(rx_chan, i2s_adc_raw_buffer, I2S_DMA_BUFFER_SIZE, NULL, portMAX_DELAY);
        //uint64_t timestamp_now = timestamp;
        uint64_t timestamp_now = System::SyncTask.ConvertHrtimToNs(timestamp);
        //printf("%llu\n", timestamp_now);
        package.timestamp = timestamp_now;

        int16_t* data_pkg = (int16_t*)pvPortMalloc(sizeof(int16_t)*package.samples_count);
        for (size_t j = 0; j < (I2S_DMA_BUFFER_SIZE/6); j++) {
            uint8_t *channel_2_data1 = &i2s_adc_raw_buffer[j * 6 + 3];// 3 bytes for channel 2
            j++;
            uint8_t *channel_2_data2 = &i2s_adc_raw_buffer[j * 6 + 3];// 3 bytes for channel 2
            data_pkg[j/2] = (int16_t)(((int32_t)unpack_24bit(channel_2_data1) + (int32_t)unpack_24bit(channel_2_data2))/2L);
            // Calculate the volume as a relative value between 0.0 and 1.0
            //float volume_1 = (float)channel_1_value / (float)MAX_VOLUME_16BIT;
            //float volume_2 = (float)channel_2_value / (float)MAX_VOLUME_16BIT;
            
            //printf("%ld\n", (int32_t)(volume_2*999.0f));
        }
        package.data = (uint8_t*)data_pkg;
        //printf("%llu\n", timestamp_now);
        if(xQueueSend(System::i2s_to_dsp_queue.getHandle(), (void*)&package, 0) != pdTRUE){
            vPortFree(data_pkg);
        }
    }
}



bool I2S_Task::i2s_receive_ISR(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    //gpio_set_level(TOGGLE_PIN, toggle_state);
    uint64_t tim_val = 0;
    uint64_t timestamp = 0;
    timestamp = System::SyncTask.GetTimeHrtim();
    gptimer_get_raw_count(i2s_timer, &tim_val);
    tim_val = tim_val % 200000;
    //timestamp -= tim_val;
    
    //toggle_state = !toggle_state;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    struct i2s_isr_arg* cfg = static_cast<struct i2s_isr_arg*>(user_ctx);
    *(cfg->timestamp) = timestamp;
    xTaskNotifyFromISR(cfg->task, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return true;
}
