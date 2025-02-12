#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "mqtt_client.h"

class MQTT_Task : public TaskBase {
public:
    MQTT_Task( ){}
    const char* TAG = "MQTT";
    virtual void Run() override;
private:
    esp_mqtt_client_handle_t client;
    void mqtt_event_handler(esp_mqtt_event_handle_t event);
    static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
};
