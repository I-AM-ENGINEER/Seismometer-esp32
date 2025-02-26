#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mesh.h"
#include "esp_err.h"
#include "esp_log.h"
#include <math.h>
#include <float.h>  // for DBL_EPSILON

// Preprocessor definitions for capture timings
#define CAPTURE_DURATION_MS   5000   // Total capture time in ms
#define CAPTURE_PERIOD_MS     20     // Sampling period in ms
#define NUM_SAMPLES           (CAPTURE_DURATION_MS / CAPTURE_PERIOD_MS)  // e.g., 250 samples

// WiFi definitions (unchanged)
#define WIFI_SSID             "TSF"
#define WIFI_PASS             "12345678"
#define USE_MESH_TSF_INSTEAD_WIFI   (true)

static const char *TAG = "wifi_app";


void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
gptimer_handle_t high_resolution_timer_init(void);

// Global high-resolution timer handle (shared among tasks/ISRs)
static gptimer_handle_t hr_timer = NULL;
double slope, intercept, tsf_offset, tim_offset;
// ---------------------------------------------------------------------------
// New Data Structures and Queue for Capture & Regression

typedef struct {
    uint64_t hr_timer_timestamp;
    uint64_t tsf_timestamp;
} gpio_timestamp_t;

typedef struct{
    uint64_t tsf_ms;
    uint64_t timer_val;
} TimingPoint_t;

// Structure to hold captured timestamp pairs
typedef struct {
    TimingPoint_t point[NUM_SAMPLES];  // High resolution timer timestamps (offset later)
    int num_samples;            // Should equal NUM_SAMPLES
} capture_data_t;

// Queue used to pass captured data from the high-priority capture task
static QueueHandle_t gpio_ts_queue = NULL;
static QueueHandle_t capture_queue = NULL;

// ---------------------------------------------------------------------------
// Theil–Sen Regression Function Implementation


// Maximum number of candidate pairs to evaluate.
#define MAX_CANDIDATES 1000

/* Comparison function for qsort, comparing doubles */
static int compare_doubles(const void *a, const void *b) {
    double diff = (*(const double*)a) - (*(const double*)b);
    return (diff < 0) ? -1 : (diff > 0);
}

/*
 * lms_approx_calc computes an approximate Least Median of Squares regression line.
 *
 * This function randomly samples candidate pairs to generate regression lines
 * and then selects the one that minimizes the median of squared residuals.
 *
 * If there are fewer than 2 points, it sets slope=0 and offset accordingly.
 * If all x values are identical, it defaults to slope=0 and offset as the median of y values.
 */
void lms_approx_calc(const TimingPoint_t* data, size_t count, double* slope, double* offset) {
    if (count < 2) {
        *slope = 0.0;
        *offset = (count == 1) ? (double)data[0].timer_val : 0.0;
        return;
    }
    
    double best_median_error = DBL_MAX;
    double best_slope = 0.0, best_offset = 0.0;
    int candidate_found = 0;
    
    // Allocate an array to hold the squared errors for one candidate.
    double *errors = malloc(count * sizeof(double));
    if (!errors) {
        *slope = 0.0;
        *offset = 0.0;
        return;
    }
    
    // Determine the total number of possible pairs.
    size_t total_pairs = count * (count - 1) / 2;
    // Use all pairs if there are few; otherwise, limit to MAX_CANDIDATES.
    size_t num_candidates = (total_pairs > MAX_CANDIDATES) ? MAX_CANDIDATES : total_pairs;
    
    // Optionally seed the random number generator once in your application:
    // srand((unsigned)time(NULL));
    
    for (size_t candidate = 0; candidate < num_candidates; candidate++) {
        // Randomly pick two distinct indices.
        size_t i = rand() % count;
        size_t j = rand() % count;
        while (j == i) {
            j = rand() % count;
        }
        
        // Skip candidate if x-values are identical.
        if (data[i].tsf_ms == data[j].tsf_ms) {
            candidate--; // Try again.
            continue;
        }
        
        double xi = (double)data[i].tsf_ms;
        double yi = (double)data[i].timer_val;
        double xj = (double)data[j].tsf_ms;
        double yj = (double)data[j].timer_val;
        
        double m = (yj - yi) / (xj - xi);
        double c = yi - m * xi;
        
        // Compute squared residuals for all points.
        for (size_t k = 0; k < count; k++) {
            double xk = (double)data[k].tsf_ms;
            double yk = (double)data[k].timer_val;
            double residual = yk - (m * xk + c);
            errors[k] = residual * residual;
        }
        
        // Sort errors to find the median squared error.
        qsort(errors, count, sizeof(double), compare_doubles);
        double median_error;
        if (count % 2 == 1)
            median_error = errors[count / 2];
        else
            median_error = (errors[count / 2 - 1] + errors[count / 2]) / 2.0;
        
        // Update the best candidate if this one is better.
        if (median_error < best_median_error) {
            best_median_error = median_error;
            best_slope = m;
            best_offset = c;
            candidate_found = 1;
        }
    }
    
    free(errors);
    
    // Fallback: If no candidate was found (e.g., all x values are identical),
    // default to slope=0 and set offset as the median of y values.
    if (!candidate_found) {
        *slope = 0.0;
        double *y_vals = malloc(count * sizeof(double));
        if (!y_vals) {
            *offset = 0.0;
            return;
        }
        for (size_t i = 0; i < count; i++) {
            y_vals[i] = (double)data[i].timer_val;
        }
        qsort(y_vals, count, sizeof(double), compare_doubles);
        if (count % 2 == 1)
            *offset = y_vals[count / 2];
        else
            *offset = (y_vals[count / 2 - 1] + y_vals[count / 2]) / 2.0;
        free(y_vals);
        return;
    }
    
    *slope = best_slope;
    *offset = best_offset;
}

uint64_t _tsf_offset, _tim_offset;

// ---------------------------------------------------------------------------
// High-Priority Capture Task
//
// This task (with priority configMAX_PRIORITIES - 1) captures a pair of
// timestamps every CAPTURE_PERIOD_MS for CAPTURE_DURATION_MS. After capture,
// it offsets the arrays by subtracting the first sample from every sample,
// then sends the capture buffer to the regression task via a queue.
void capture_task(void* arg) {
    while(1){
        capture_data_t *data = malloc(sizeof(capture_data_t));
        if (data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate capture_data_t");
            vTaskDelete(NULL);
        }
        data->num_samples = NUM_SAMPLES;

        // Get the current tick count for precise periodic sampling
        TickType_t xLastWakeTime = xTaskGetTickCount();
        for (int i = 0; i < NUM_SAMPLES; i++) {
            ESP_ERROR_CHECK(gptimer_get_raw_count(hr_timer, &data->point[i].timer_val));
            data->point[i].tsf_ms = esp_mesh_get_tsf_time();
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(CAPTURE_PERIOD_MS));
        }

        // Offset each array by its first value so that the first sample becomes zero.
        _tim_offset = data->point[0].timer_val;
        _tsf_offset = data->point[0].tsf_ms;
        for (int i = 0; i < NUM_SAMPLES; i++) {
            data->point[i].timer_val -= _tim_offset;
            data->point[i].tsf_ms -= _tsf_offset;
        }

        if(xQueueSend(capture_queue, &data, 0) != pdTRUE){
            free(data);
        }
    }
}

// ---------------------------------------------------------------------------
// Low-Priority Regression Task
//
// This task (with near-idle priority) waits for a pointer to a captured data
// buffer, then computes the Theil–Sen regression coefficients (slope and intercept)
// on the (offset) data. Since the regression computation is time-consuming and
// not real-time critical, it runs at low priority.
void regression_task(void* arg) {
    capture_data_t *data;
    for (;;) {
        if (xQueueReceive(capture_queue, &data, portMAX_DELAY) == pdTRUE) {
            double new_slope;
            double new_intercept;
            lms_approx_calc(data->point, data->num_samples, &new_slope, &new_intercept);
            //vPortEnterCritical();
            tim_offset = _tim_offset;
            tsf_offset = _tsf_offset;
            intercept = new_intercept;
            slope = new_slope;
            //vPortExitCritical();
            free(data);
        }
    }
}


/**
 * @brief Approximates the TSF (time sync frame) timestamp for a given timer timestamp.
 *
 * This function assumes that the regression model was computed using offset values:
 *   (timer - tim_offset) = slope * (tsf - tsf_offset) + intercept.
 *
 * @param timer_timestamp The raw timer timestamp.
 * @return The approximated TSF timestamp.
 */
double get_approx_tsf(uint64_t timer_timestamp) {
    // Avoid division by zero in case the regression slope is zero.
    if (slope == 0.0) {
        // Fallback: return the TSF offset.
        return (double)tsf_offset;
    }
    
    // Convert the raw timer timestamp to its offset value.
    double timer_val_offset = (double)timer_timestamp - (double)tim_offset;
    
    // Invert the regression equation to estimate the offset TSF value.
    double tsf_est_offset = (timer_val_offset - intercept) / slope;
    
    // Convert back to an absolute TSF value by adding the TSF offset.
    return (double)tsf_offset + tsf_est_offset;
}

// ISR for GPIO4 – note IRAM_ATTR so that it is placed in IRAM
void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint64_t timestamp_tim, timestamp_tsf;
    // Get the current high-resolution timer count
    
    // Get the current TSF timestamp
    timestamp_tsf = esp_mesh_get_tsf_time();
    gptimer_get_raw_count(hr_timer, &timestamp_tim);


    // Allocate memory for the structure in IRAM
    gpio_timestamp_t* ts_data = (gpio_timestamp_t*) pvPortMalloc(sizeof(gpio_timestamp_t));
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
            printf("%.3f\t%llu\n", 
                    (double)get_approx_tsf(ts_data->hr_timer_timestamp), 
                   ts_data->tsf_timestamp);
            vPortFree(ts_data); // Free memory after processing
        }
    }
}

// ---------------------------------------------------------------------------
// WiFi and IP Event Handler (unchanged)
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
        }
    }
}

// ---------------------------------------------------------------------------
// High-Resolution Timer Initialization (unchanged)
gptimer_handle_t high_resolution_timer_init(void) {
    gptimer_handle_t timer_handle;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 40000000,  // 40MHz → 25ns per tick
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &timer_handle));
    ESP_ERROR_CHECK(gptimer_enable(timer_handle));
    ESP_ERROR_CHECK(gptimer_start(timer_handle));
    return timer_handle;
}

// ---------------------------------------------------------------------------
// Main Application Entry Point
void app_main(void) {
    // Initialize NVS, networking, and event loop
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

    // Initialize the high-resolution timer (used by multiple tasks/ISRs)
    hr_timer = high_resolution_timer_init();

    // Create a queue to pass captured data from capture_task to regression_task
    capture_queue = xQueueCreate(1, sizeof(capture_data_t *));
    if (capture_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create capture queue");
        return;
    }

    // Create a queue to hold timestamps (queue length 10)
    // Queue to hold pointers to gpio_timestamp_t structures
    gpio_ts_queue = xQueueCreate(10, sizeof(gpio_timestamp_t*));

    if (gpio_ts_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create GPIO timestamp queue");
        return;
    }

    // Create the low-priority regression task (near idle priority)
    xTaskCreate(regression_task, "regression_task", 8192, NULL, tskIDLE_PRIORITY + 1, NULL);

    // Create the high-priority capture task (priority MAX-1)
    xTaskCreate(capture_task, "capture_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL);

    // Create the task that will print timestamps received from the GPIO ISR
    xTaskCreate(gpio_print_task, "GPIO_Print_Task", 2048, NULL, 5, NULL);
    // (Optional) Configure GPIO interrupt, TSF task, etc. as needed...
    // For example, if you still need the GPIO ISR:
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    // Here we set maximum ISR priority (level 5) so that the ISR is as fast as possible.
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_4, gpio_isr_handler, (void*) GPIO_NUM_4));

    // The rest of your app_main continues...
}
