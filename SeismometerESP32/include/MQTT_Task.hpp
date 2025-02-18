#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "mqtt_client.h"

class MQTT_Task : public TaskBase {
public:
    MQTT_Task( ){}
    static constexpr char TAG[] = "MQTT";
    virtual void Run( void* arg ) override;
private:
    esp_mqtt_client_handle_t client;
    static void event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
};
