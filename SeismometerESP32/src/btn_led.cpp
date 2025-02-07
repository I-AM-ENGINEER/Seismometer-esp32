#include "system.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/list.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "config.h"

static const char *TAG = "BTN_LED";
#define QUEUE_SIZE  10

#define GPIO_INPUT_IO         GPIO_NUM_4
#define GPIO_INPUT_PIN_SEL    (1ULL << GPIO_INPUT_IO)
#define ESP_INTR_FLAG_DEFAULT 0


//#define TIMER_INTERVAL_MS     100  // 100ms


/*
// Global queue handle for passing event times (each event is a uint64_t timestamp)
static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg){
    //uint64_t event_time = //hrtim_to_tsf();
    //xQueueSendFromISR(gpio_evt_queue, &event_time, NULL);
}


void gpio_event_task(void *arg){
    gpio_evt_queue = xQueueCreate(QUEUE_SIZE, sizeof(uint64_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    uint64_t event_time;
    while(1) {
        if (xQueueReceive(gpio_evt_queue, &event_time, portMAX_DELAY)) {
            //printf("%llu\n", event_time);
            //ESP_LOGI(TAG, "External interrupt event detected at time: %llu", event_time);
        }
    }
}

void gpio_isr_init( void ){
    // Create the queue to hold event timestamps
    gpio_evt_queue = xQueueCreate(QUEUE_SIZE, sizeof(uint64_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // Create the task that will process and print event timestamps.
    xTaskCreate(gpio_event_task, "gpio_event_task", 2048, NULL, 10, NULL);

    // Configure the GPIO pin as input with an interrupt on the rising edge.
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_POSEDGE;   // Interrupt on rising edge
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;                  // Enable pull-up resistor (adjust as needed)
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Install the GPIO ISR service.
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // Add the ISR handler for the specified GPIO pin.
    gpio_isr_handler_add(GPIO_INPUT_IO, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "External interrupt task started. Waiting for events...");
}

*/