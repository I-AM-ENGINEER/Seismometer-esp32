#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "driver/gpio.h"

class BTN_CheckerTask : public TaskBase {
public:
    virtual void Run( void* arg ) override {
        gpio_reset_pin(GPIO_BTN_PIN);
        gpio_set_direction(GPIO_BTN_PIN, GPIO_MODE_INPUT);
        gpio_set_pull_mode(GPIO_BTN_PIN, GPIO_PULLDOWN_ONLY);
        
        while (1){
            vTaskDelay(pdMS_TO_TICKS(100));;
            //printf("%lu\n", (uint32_t)xPortGetFreeHeapSize());
        }
    }
};
