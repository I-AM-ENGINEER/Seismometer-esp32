#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "driver/gpio.h"

class DSP_Task : public TaskBase {
public:
    DSP_Task( ){}
    const char* TAG = "DSP";
    virtual void Run() override;

    void SetGain( sound_frame_gain_t gain ){
        
    }

private:
    int16_t unpack_24bit(uint8_t* data){
        int16_t val = (data[2] << 8) | data[1];
        return val;
    }
};
