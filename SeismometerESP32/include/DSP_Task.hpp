#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "driver/gpio.h"
#include <vector>
#include <cstdint>


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
    //void* PackSample( SampleHeader_t header, uint8_t* samples_buff, size_t samples_buff_size );
    uint8_t* PackNetPackage(NetPackageHeader_t& net_header, const std::vector<std::pair<SampleHeader_t, uint8_t*>>& samples);
};
