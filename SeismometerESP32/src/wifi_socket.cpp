//#include "wifi_task.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "config.h"
#include "esp_log.h"
#include "system.hpp"
//#include "i2s_task.h"

/*

static const char *TAG = "WIFI";
volatile bool wifi_connected;
//extern QueueHandle_t package_send_queue;

static void wifi_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data ) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_connected = true;
        ESP_LOGI(TAG, "Connected to Wi-Fi network.");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI(TAG, "Disconnected from Wi-Fi network.");
        esp_wifi_connect();
    } 
}


int create_and_connect_socket(const char *ip, int port) {
    int sock;
    struct sockaddr_in destAddr;

    // Configure the destination address
    destAddr.sin_addr.s_addr = inet_addr(ip);
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);

    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return -1;
    }
    ESP_LOGI(TAG, "Socket created");


    int flag = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0) {
        ESP_LOGE(TAG, "Failed to set TCP_NODELAY: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "TCP_NODELAY option set successfully");
    }
    

    //int buf_size = 24000;  // Set to an appropriate size for large packets
    //setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));

    //int flags = fcntl(sock, F_GETFL, 0);
    //fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Attempt to connect
    if (connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        close(sock);
        return -1;
    }

    ESP_LOGI(TAG, "Successfully connected to %s:%d", ip, port);
    return sock;
}

extern QueueHandle_t i2s_raw_adc_queue;
static i2s_dma_package_t pkg;

void tcp_send_task( void *pvParameters ) {
    int sock = -1;
    int reconnect_attempts = 0;

    while (1) {
        if (sock < 0) {
            ESP_LOGI(TAG, "Attempting to reconnect...");
            sock = create_and_connect_socket(IP_TARGET, IP_TARGET_PORT);
            if (sock >= 0) {
                reconnect_attempts = 0;
            } else {
                reconnect_attempts++;
                int delay = reconnect_attempts * TCP_RECONNECT_DELAY_MS;
                if (delay > TCP_MAX_RETRY_DELAY_MS){
                    delay = TCP_MAX_RETRY_DELAY_MS;
                }
                ESP_LOGW(TAG, "Reconnect failed, retrying in %d ms...", delay);
                vTaskDelay(pdMS_TO_TICKS(delay));
                continue;
            }
        }

        //wifi_status_t status = esp_wifi_get_status();
        //if (status.tx_queue_length < MAX_TX_QUEUE_LENGTH) {
        ESP_LOGI(TAG, "Receiving...");
        xQueueReceive(i2s_raw_adc_queue, &pkg, portMAX_DELAY);
        if(send(sock, &pkg, sizeof(pkg), 0) < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            close(sock);
            sock = -1;
        }
    }
}


void wifi_init_sta( void ) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "5G_COVID_BASE_STATION",
            .password = "1SimplePass9",
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            }
        },
    };
    esp_wifi_set_max_tx_power(78); // Max value 78 corresponds to ~20 dBm
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}*/
