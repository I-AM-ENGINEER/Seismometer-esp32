#pragma once
#include <vector>
#include "config.h"
#include "SystemStructs.hpp"
#include "driver/gpio.h"

class LedTask : public TaskBase {
public:
    using LedStep     = std::pair<bool, uint32_t>;
    using LedSequence = std::vector<LedStep>;
    LedTask( gpio_num_t pin, bool invert = true )
        : _pin(pin), _invert(invert) {
            _led_queue = xQueueCreate(3, sizeof(LedSequence*));
        }

    virtual void Run( void* arg ) override {
        gpio_reset_pin(_pin);
        gpio_set_direction(_pin, GPIO_MODE_OUTPUT);
        LedSequence current_sequence = { { false, UINT32_MAX } };
        while (1) {
            for(auto &step : current_sequence){
                gpio_set_level(_pin, _invert ? !step.first : step.first);
                TickType_t delay_ticks = (step.second == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(step.second);
                LedSequence* next_sequence_ptr = nullptr;
                if(xQueueReceive(_led_queue, &next_sequence_ptr, delay_ticks) == pdPASS){
                    current_sequence = *next_sequence_ptr;
                    delete next_sequence_ptr;
                    break;
                }
            }
        }
    }

    void SetStatic( bool state ){
        LedSequence static_val = { { state, UINT32_MAX } };
        SetSequence(static_val);
    }
    
    void SetSequence( const LedSequence &sequence ){
        LedSequence* seq_ptr = new LedSequence(sequence);
        xQueueSend(_led_queue, &seq_ptr, portMAX_DELAY);
    }

private:
    gpio_num_t _pin;
    bool _invert;
    QueueHandle_t _led_queue;
};
