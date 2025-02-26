#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mesh.h"
#include "esp_err.h"
#include "esp_log.h"
#include "string.h"
#include "stdio.h"

#define USE_MESH_TSF_INSTEAD_WIFI   (true)
#define WIFI_SSID                   "TSF"
#define WIFI_PASS                   "12345678"

static const char *TAG = "wifi_app";

typedef struct {
    uint64_t hr_timer_timestamp;
    uint64_t tsf_timestamp;
} gpio_timestamp_t;


// Forward declarations
void tsf_task(void* arg);
static void IRAM_ATTR gpio_isr_handler(void* arg);
void gpio_print_task(void* arg);
gptimer_handle_t high_resolution_timer_init(void);

// Global handles
static QueueHandle_t gpio_ts_queue = NULL;
static gptimer_handle_t hr_timer = NULL; // Global high resolution timer used by both tasks and ISR

// Initialize a high-resolution timer (40MHz resolution → 25ns tick)
gptimer_handle_t high_resolution_timer_init(void) {
    gptimer_handle_t timer_handle;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 40000000,  // 40MHz
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &timer_handle));
    ESP_ERROR_CHECK(gptimer_enable(timer_handle));
    ESP_ERROR_CHECK(gptimer_start(timer_handle));
    return timer_handle;
}

// This task (unchanged from your code) prints TSF values
void tsf_task(void* arg) {
    esp_log_level_set("*", ESP_LOG_NONE); // Disable log for clean capture
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("\n***START TSF LOG***\n");

    // Use the already initialized global high-resolution timer
    TickType_t task_timestamp = xTaskGetTickCount();
    while (1) {
        uint64_t timestamp_tsf;
        uint64_t timestamp_hrtim_start, timestamp_hrtim_end;
        ESP_ERROR_CHECK(gptimer_get_raw_count(hr_timer, &timestamp_hrtim_start));
        #if (USE_MESH_TSF_INSTEAD_WIFI)
            timestamp_tsf = esp_mesh_get_tsf_time();
        #else
            timestamp_tsf = esp_wifi_get_tsf_time(WIFI_IF_STA);
        #endif
        ESP_ERROR_CHECK(gptimer_get_raw_count(hr_timer, &timestamp_hrtim_end));
        printf("%llu\t%llu\t%llu\n",
               timestamp_hrtim_start,
               timestamp_tsf,
               timestamp_hrtim_end);
        vTaskDelayUntil(&task_timestamp, pdMS_TO_TICKS(100));
    }
}

// ISR for GPIO4 – note IRAM_ATTR so that it is placed in IRAM
void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint64_t timestamp_tim, timestamp_tsf;
    // Get the current high-resolution timer count
    
    // Get the current TSF timestamp
    timestamp_tsf = esp_mesh_get_tsf_time();
    gptimer_get_raw_count(hr_timer, &timestamp_tim);


    // Allocate memory for the structure in IRAM
    gpio_timestamp_t* ts_data = (gpio_timestamp_t*) heap_caps_malloc(sizeof(gpio_timestamp_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ts_data == NULL) {
        return; // Failed to allocate memory
    }

    ts_data->hr_timer_timestamp = timestamp_tim;
    ts_data->tsf_timestamp = timestamp_tsf;

    // Send the pointer to the structure to the queue
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gpio_ts_queue, &ts_data, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


// Task that waits on the queue and prints both timestamps
void gpio_print_task(void* arg) {
    gpio_timestamp_t* ts_data;
    vTaskDelay(pdMS_TO_TICKS(10000));
    printf("\n***START LOG***\n");
    while (1) {
        if (xQueueReceive(gpio_ts_queue, &ts_data, portMAX_DELAY) == pdTRUE) {
            printf("%llu\n", 
                   //ts_data->hr_timer_timestamp, 
                   ts_data->tsf_timestamp);
            heap_caps_free(ts_data); // Free memory after processing
        }
    }
}


// WiFi and IP event handler (unchanged from your code)
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi started, connecting to AP...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Disconnected from AP, retrying...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Start TSF capturing");
            //xTaskCreate(tsf_task, "TSF", 4000, NULL, configMAX_PRIORITIES - 1, NULL);
        }
    }
}

void app_main(void) {
    // Initialize NVS, network interfaces, and event loop
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Register WiFi and IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize the global high-resolution timer
    hr_timer = high_resolution_timer_init();

    // Create a queue to hold timestamps (queue length 10)
    // Queue to hold pointers to gpio_timestamp_t structures
    gpio_ts_queue = xQueueCreate(10, sizeof(gpio_timestamp_t*));

    if (gpio_ts_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create GPIO timestamp queue");
        return;
    }

    // Create the task that will print timestamps received from the GPIO ISR
    xTaskCreate(gpio_print_task, "GPIO_Print_Task", 2048, NULL, 5, NULL);

    // Configure GPIO4 as an input with a rising edge interrupt.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,      // Interrupt on rising edge
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_4),  // Only GPIO4
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // Enable pull-up if needed
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Install the GPIO ISR service and add our ISR handler for GPIO4
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_4, gpio_isr_handler, (void*) GPIO_NUM_4));

    // The rest of your application continues (TSF task will start on IP event).
}
