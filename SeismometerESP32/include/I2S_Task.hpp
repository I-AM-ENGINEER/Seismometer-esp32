#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <cstdlib>
#include <cstring>

#define I2S_WS_PIN GPIO_NUM_21

#define MAX_VOLUME_16BIT (32768)  // Maximum absolute value for a 24-bit signed integer

constexpr i2s_std_config_t i2s_rx_config = {
    .clk_cfg  = {
        .sample_rate_hz = I2S_SAMPLERATE,
        .clk_src = I2S_CLK_SRC_PLL_160M,
        .ext_clk_freq_hz = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_384,
    },
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_STEREO),

    .gpio_cfg = {
        .mclk = GPIO_NUM_20,
        .bclk = GPIO_NUM_9,
        .ws   = I2S_WS_PIN,
        .dout = I2S_GPIO_UNUSED,
        .din  = GPIO_NUM_10,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};

constexpr i2s_chan_config_t rx_chan_cfg = {
    .id = I2S_NUM_AUTO,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 6,
    .dma_frame_num = I2S_DMA_FRAMES,
    .auto_clear = false,
    .intr_priority = 7,
};

constexpr gptimer_config_t i2s_tim_cfg = {
    .clk_src = GPTIMER_CLK_SRC_APB,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 40'000'000,
    .intr_priority = 0,
    .flags = {
        .intr_shared = 0,
        .allow_pd = 0
    }
};

class I2S_Task : public TaskBase {
public: 
    I2S_Task( ){}
    //static uint8_t* i2s_isr_pkg_buff;
    i2s_chan_handle_t    rx_chan;
    const char* TAG = "I2S";
    virtual void Run( void* arg ) override;

    void SetGain( sound_frame_gain_t gain ){
        
    }

private:
    struct i2s_isr_arg{
        TaskHandle_t task;
        uint64_t* timestamp;
    };

    static bool IRAM_ATTR i2s_receive_ISR(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
    void setup_ws_interrupt(void);
    int16_t unpack_24bit(uint8_t* data){
        int16_t val = (data[2] << 8) | data[1];
        return val;
    }
};
