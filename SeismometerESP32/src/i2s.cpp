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

static const char *TAG = "I2S";

void I2S_Task::Run() {
    i2s_event_callbacks_t callbacks = {
        .on_recv            = i2s_receive_ISR,
        .on_recv_q_ovf      = NULL,
        .on_sent            = NULL,
        .on_send_q_ovf      = NULL,
    };
    uint16_t* single_channel_buff = (uint16_t*)pvPortMalloc(sizeof(uint16_t)*I2S_SAMPLES_MONO_FRAME);
    if(single_channel_buff == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }
    uint8_t* i2s_adc_isr_buffer = (uint8_t*)pvPortMalloc(I2S_DMA_BUFFER_SIZE);
    if(i2s_adc_isr_buffer == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }
    uint8_t* i2s_adc_raw_buffer = (uint8_t*)pvPortMalloc(I2S_DMA_BUFFER_SIZE);
    if(i2s_adc_raw_buffer == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }


    gpio_set_direction(GPIO_GAIN_1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_10_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_100_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GAIN_1000_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_GAIN_1_PIN, 0);
    gpio_set_level(GPIO_GAIN_10_PIN, 1);
    gpio_set_level(GPIO_GAIN_100_PIN, 0);
    gpio_set_level(GPIO_GAIN_1000_PIN, 0);


    volatile uint64_t timestamp;
    struct i2s_isr_arg isr_arg = {
        .task = _handle,
        .rcv_buff = i2s_adc_isr_buffer,
        .timestamp = (uint64_t*)&timestamp
    };

    ESP_LOGI(TAG, "I2S initializing...");
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &i2s_rx_config));
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_chan, &callbacks, &isr_arg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    i2s_channel_read(rx_chan, i2s_adc_raw_buffer, I2S_DMA_BUFFER_SIZE, NULL, 0);
    while (1) {       
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (size_t j = 0; j < (I2S_DMA_BUFFER_SIZE/6); j++) {
            //uint8_t *channel_1_data = &i2s_adc_isr_buffer[j * 6];          // 3 bytes for channel 1
            uint8_t *channel_2_data = &i2s_adc_isr_buffer[j * 6 + 3];      // 3 bytes for channel 2
            //int16_t channel_1_value = unpack_24bit(channel_1_data);
            int16_t channel_2_value = unpack_24bit(channel_2_data);
            
            single_channel_buff[j] = channel_2_value;
            // Calculate the volume as a relative value between 0.0 and 1.0
            //float volume_1 = (float)channel_1_value / (float)MAX_VOLUME_16BIT;
            //float volume_2 = (float)channel_2_value / (float)MAX_VOLUME_16BIT;
            
            //printf("%ld\n", (int32_t)(volume_2*999.0f));
        }

        uint8_t* data_pkg = (uint8_t*)pvPortMalloc(sizeof(uint16_t)*I2S_SAMPLES_MONO_FRAME);
        I2S_to_DSP_package_t package = {
            .data = data_pkg,
            .samples_count = I2S_SAMPLES_MONO_FRAME,
            .timestamp = timestamp,
            .channels_count = 1,
            .bits_per_single_sample = 16
        };
        
        xQueueSend(System::i2s_to_dsp_queue.getHandle(), (void*)&package, portMAX_DELAY);
    }
}

bool I2S_Task::i2s_receive_ISR(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    uint64_t timestamp = System::SyncTask.GetTime_ns();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    struct i2s_isr_arg* cfg = static_cast<struct i2s_isr_arg*>(user_ctx);
    *(cfg->timestamp) = timestamp;
    memcpy(cfg->rcv_buff, event->data, event->size);
    xTaskNotifyFromISR(cfg->task, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return pdTRUE;
}
