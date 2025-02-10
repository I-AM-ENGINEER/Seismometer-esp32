#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <cstdint>
#include <memory>

#include "SystemStructs.hpp"
#include "PrecisionTimeTask.hpp"
#include "LedTask.hpp"
#include "BtnTask.hpp"
#include "WiFiTask.hpp"

namespace LedSequence{
    static LedTask::LedSequence unconnected    = { { true, 400 }, { false, 600 } };
    static LedTask::LedSequence connected_WiFi = { { true, 100 }, { false, 100 } };
    static LedTask::LedSequence connected_MQTT = { { true, 100 }, { false, 400 } };
}

class StateMachine;

class System {
public:
    static void Start( void );

//private:
    //PrecisionTimeTask _TimeTask;
    static LedTask Led;
    static WiFiTask WiFi;
    static StateMachine StateMch;
    static PrecisionTimeTask SyncTask;
    static MessageQueue<I2S_To_DSP_Package_t> _i2s_to_dsp_queue;
    static MessageQueue<NetQueueElement_t> _dsp_to_mqtt_queue;
};

class StateMachine : public TaskBase {
public:
    typedef enum{
        STATE_UNCONNECTED = 0,
        STATE_CONNECTED_WIFI,
        STATE_CONNECTED_MQTT,
    } state_t;

    StateMachine() {}
    
    virtual void Run() override {
        _current_state = STATE_UNCONNECTED;
        System::Led.SetSequence(LedSequence::unconnected);
        while(1){
            switch (_current_state){
                case STATE_UNCONNECTED:
                    if(System::WiFi.IsConnected()){
                        _current_state = STATE_CONNECTED_WIFI;
                        System::Led.SetSequence(LedSequence::connected_WiFi);
                    }
                    break;
                case STATE_CONNECTED_WIFI:
                    if(!System::WiFi.IsConnected()){
                        _current_state = STATE_UNCONNECTED;
                        System::Led.SetSequence(LedSequence::unconnected);
                    }
                    break;
                default: break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
private:
    state_t _current_state;
};

//void wifi_init_sta( void );
