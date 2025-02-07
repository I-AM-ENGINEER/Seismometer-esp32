#pragma once

#include "SystemStructs.hpp"

class PrecisionTimeTask : public TaskBase {
public:
    PrecisionTimeTask() {

    }
    
    virtual void Run() override {
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
};

