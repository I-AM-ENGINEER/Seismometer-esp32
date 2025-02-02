#include "dsp_task.h"
#include "i2s_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "wifi_task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DSP";

extern QueueHandle_t i2s_raw_adc_queue;
extern QueueHandle_t package_send_queue;

//uint8_t i2s_adc_raw_buffer;

//extern QueueHandle_t i2s_raw_adc_queue;
static i2s_dma_package_t pkg;

// Unpack 24-bit data to 32-bit signed integer


void dsp_task( void *arg ){
    uint8_t i = 0;
    while (1){
        memset(&pkg, 0, sizeof(pkg));
        xQueueReceive(i2s_raw_adc_queue, &pkg, portMAX_DELAY);
        ESP_LOGI(TAG, "!!!");
        i+=10;
        /*

        if(i == 0){
            for(size_t j = 0; j < sizeof(pkg.data); j++){
                if(j%6 == 0){
                    printf("\n");
                }
                //printf("%02hhX", pkg.data[j]);
            }
        }*/
    }
    
}
/*
static tcp_packet_t packet;
static i2s_dma_package_t pkg;

char str[] = "fdsafsdfsdfsdfsdfsfafsfsddfasfsddffdfasdfddfsasfdf"; 

void dsp_task( void *arg ){
    //uint64_t last_timestamp = 0;
    while (1){
        //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        memset(&pkg, 0, sizeof(pkg));
        //xQueueReceive(i2s_raw_adc_queue, &pkg, portMAX_DELAY);
        
        //packet.data
        memcpy(packet.data, pkg.data, sizeof(packet.data));
        //memcpy(packet.data, &pkg.timestamp, sizeof(pkg.timestamp));
        //memcpy(packet.data, str, sizeof(str));
        //memset(packet.data, 'A', sizeof(packet.data));
        xQueueSend(package_send_queue, &packet, portMAX_DELAY);
        //ESP_LOGI(TAG, "!!!!!!");
    }
}*/
