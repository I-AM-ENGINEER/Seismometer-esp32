#include "mqtt_client.h"
#include "lwip/netdb.h"
#include "MQTT_Task.hpp"
#include "System.hpp"
#include "esp_log.h"

static bool is_mqtt_connected = false;

void MQTT_Task::event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    auto *self = static_cast<MQTT_Task *>(handler_args);
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            is_mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            is_mqtt_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
}


void MQTT_Task::Run( void* arg ) {
    struct addrinfo hints = {}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    while (!System::WiFi.IsConnected()){
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    //vTaskDelay(pdMS_TO_TICKS(3000));
    esp_netif_dns_info_t dns;
    dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
    dns.ip.type = IPADDR_TYPE_V4;
    esp_netif_set_dns_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), ESP_NETIF_DNS_MAIN, &dns);


    while (getaddrinfo(IP_TARGET, nullptr, &hints, &res) != 0) {
        ESP_LOGE(TAG, "DNS resolution failed, retrying in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip_str, sizeof(ip_str));
    freeaddrinfo(res);
    
    ESP_LOGI(TAG, "Resolved IP: %s", ip_str);
    
    char mqtt_uri[64];
    snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%d", ip_str, IP_TARGET_PORT);
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = mqtt_uri;
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    mqtt_cfg.network.disable_auto_reconnect = false;
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, event_handler, this);
    esp_mqtt_client_start(client);
    
    while (1) {
        if (!System::WiFi.IsConnected()) {
            ESP_LOGW(TAG, "WiFi disconnected, stopping MQTT client...");
            esp_mqtt_client_stop(client);
            while (!System::WiFi.IsConnected()){
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            vTaskDelay(pdMS_TO_TICKS(3000));
            ESP_LOGI(TAG, "WiFi reconnected, restarting MQTT client...");
            esp_mqtt_client_start(client);
        }
        
        while (!is_mqtt_connected) {
            ESP_LOGW(TAG, "MQTT client not connected, restarting MQTT client...");
            esp_mqtt_client_start(client);
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        NetQueueElement_t pkg;
        xQueueReceive(System::dsp_to_mqtt_queue.getHandle(), &pkg, portMAX_DELAY);

        int msg_id = esp_mqtt_client_publish(client, "xui", (const char*)pkg.data, pkg.package_size, 0, 0);
        if (msg_id == -1) {
            ESP_LOGW(TAG, "Publish failed, client may not be connected yet.");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vPortFree(pkg.data);
    }
}
