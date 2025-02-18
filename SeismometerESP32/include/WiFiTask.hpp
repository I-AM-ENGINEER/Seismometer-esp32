#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "esp_wifi.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>

class WiFiTask : public TaskBase {
public:
    WiFiTask( void ) {}
    
    virtual void Run( void* arg ) override {
        esp_wifi_set_ps(WIFI_PS_NONE); // No power save

        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, &instance_any_id);

        wifi_config_t wifi_config;
        esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), _SSID);
        strcpy(reinterpret_cast<char*>(wifi_config.sta.password), _PSK);


        esp_wifi_set_max_tx_power(78); // Max value 78 corresponds to ~20 dBm
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        vTaskDelete(NULL);
        //while (true) {
        //    vTaskDelay(pdMS_TO_TICKS(1000));
        //}
    }

    void SetCredits( const char* SSID, const char* PSK ){
        size_t ssid_len = std::min(static_cast<size_t>(sizeof(wifi_sta_config_t::ssid)), strlen(SSID)+1);
        size_t psk_len = std::min(static_cast<size_t>(sizeof(wifi_sta_config_t::password)), strlen(PSK)+1);
        _SSID = static_cast<char*>(pvPortMalloc(ssid_len));
        _PSK = static_cast<char*>(pvPortMalloc(psk_len));
        strcpy(_SSID, SSID);
        strcpy(_PSK, PSK);
        ESP_LOGI("WiFi", "New SSID %s and PSK setted", _SSID);
    }

    bool IsConnected( void ){
        return _connection_state;
    }
private:
    bool _connection_state = false;
    char* _SSID = nullptr;
    char* _PSK = nullptr;


    static void wifi_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data ) {
        WiFiTask* WiFi_ptr = static_cast<WiFiTask*>(arg);
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
            WiFi_ptr->_connection_state = true;
            ESP_LOGI("WiFi", "Connected to Wi-Fi network.");
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            WiFi_ptr->_connection_state = false;
            ESP_LOGI("WiFi", "Disconnected from Wi-Fi network.");
            esp_wifi_connect();
        } 
    }
};

