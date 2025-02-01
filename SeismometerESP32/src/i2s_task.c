#include "i2s_task.h"

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

static const char *TAG = "I2S";

extern TaskHandle_t         dsp_task_handle;
extern QueueHandle_t        i2s_raw_adc_queue;
extern volatile bool        wifi_connected;

extern TaskHandle_t i2s_task_handle;

static i2s_chan_handle_t    rx_chan;

#define MAX_VOLUME_24BIT (32768)  // Maximum absolute value for a 24-bit signed integer
int16_t unpack_24bit(uint8_t* data){
    int16_t val = (data[2] << 8) | data[1];
    return val;
}

i2s_std_config_t i2s_rx_config = {
    .clk_cfg  = {
        .sample_rate_hz = I2S_SAMPLERATE,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_STEREO),
    /*.slot_cfg = { //I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_STEREO),
        .data_bit_width = I2S_DATA_BIT_WIDTH_24BIT, \
        .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, \
        .slot_mode = I2S_SLOT_MODE_STEREO, \
        .slot_mask = I2S_STD_SLOT_BOTH, \
        .ws_width = 25, \
        .ws_pol = false, \
        .bit_shift = true, \
        .left_align = false, \
        .big_endian = false, \
        .bit_order_lsb = false \
    },*/
    .gpio_cfg = {
        .mclk = GPIO_NUM_20,
        .bclk = GPIO_NUM_9,
        .ws   = GPIO_NUM_21,
        .dout = I2S_GPIO_UNUSED,
        .din  = GPIO_NUM_10,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};

i2s_chan_config_t rx_chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 6,
    .dma_frame_num = I2S_DMA_FRAMES,
    .auto_clear = false,
    .intr_priority = 3,
};

static i2s_dma_package_t* pkg;
static uint8_t* i2s_adc_raw_buffer;
//static QueueHandle_t i2s_isr_queue;

static uint8_t *i2s_isr_pkg_buff;

static bool IRAM_ATTR i2s_receive_ISR( i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx ){
    static int sended_packs = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    memcpy(&i2s_isr_pkg_buff[I2S_DMA_BUFFER_SIZE * sended_packs], event->data, I2S_DMA_BUFFER_SIZE);
    sended_packs++;
    if(sended_packs == I2S_DMA_FRAMES_BUFFER){
        sended_packs = 0;
        xTaskNotifyFromISR(i2s_task_handle, 0, eNoAction, pdFALSE);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return pdTRUE;
}

void i2s_task( void *arg ){
    i2s_event_callbacks_t callbacks = {
        .on_recv            = i2s_receive_ISR,
        .on_recv_q_ovf      = NULL,
        .on_sent            = NULL,
        .on_send_q_ovf      = NULL,
    };

    //i2s_isr_queue = xQueueCreate(8, I2S_DMA_BUFFER_SIZE);
    i2s_isr_pkg_buff = malloc(I2S_DMA_BUFFER_SIZE * I2S_DMA_FRAMES_BUFFER);

    i2s_adc_raw_buffer = (uint8_t*)malloc(I2S_DMA_BUFFER_SIZE);//I2S_RAW_BUFFER_SIZE
    if(i2s_adc_raw_buffer == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }
    pkg = (i2s_dma_package_t*)malloc(sizeof(i2s_dma_package_t));
    if(pkg == NULL){
        ESP_LOGE(TAG, "FAILED BUFFER ALLOCATION");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "I2S initializing...");
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &i2s_rx_config));
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_chan, &callbacks, NULL));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    
    //2s_channel_read(rx_chan, i2s_adc_raw_buffer, I2S_DMA_BUFFER_SIZE, NULL, portMAX_DELAY);

    //i2s_get_buf_size(rx_chan, i2s_rx_config.slot_cfg.data_bit_width, )
    i2s_channel_read(rx_chan, i2s_adc_raw_buffer, I2S_DMA_BUFFER_SIZE, NULL, 0);
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        //printf("%u\n", ffffffff);
        
        int64_t tsf_time = esp_wifi_get_tsf_time(ESP_IF_WIFI_STA);
        portENTER_CRITICAL(NULL);
        memcpy(pkg->data, i2s_isr_pkg_buff, I2S_DMA_BUFFER_SIZE * I2S_DMA_FRAMES_BUFFER);
        portEXIT_CRITICAL(NULL);
        pkg->timestamp = (uint64_t)tsf_time;
        


        for (size_t j = 0; j < sizeof(pkg->data); j += 6) {
            uint8_t *channel_1_data = &pkg->data[j];          // 3 bytes for channel 1
            uint8_t *channel_2_data = &pkg->data[j + 3];      // 3 bytes for channel 2
            
            int32_t channel_1_value = unpack_24bit(channel_1_data);
            int32_t channel_2_value = unpack_24bit(channel_2_data);

            // Calculate the volume as a relative value between 0.0 and 1.0
            float volume_1 = (float)channel_1_value / (float)MAX_VOLUME_24BIT;
            float volume_2 = (float)channel_2_value / (float)MAX_VOLUME_24BIT;
            
            // Print the relative volume for each channel
            printf("%.6f %.6f\n", volume_1, volume_2);
            //printf("%02hhX%02hhX%02hhX", channel_1_data[0], channel_1_data[1], channel_1_data[2]);
            //printf("%hhu %hhu %hhu %hhu %hhu %hhu\n", channel_1_data[0], channel_1_data[1], channel_1_data[2], channel_2_data[0], channel_2_data[1], channel_2_data[2]);
        }


        //ESP_LOGI(TAG, "I2S readed!");
        //printf("%lld\n", tsf_time);
        //vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

