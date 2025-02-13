#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "esp_mesh.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/gptimer.h"
#include <array>
#include <cstddef>

#define WIFI_TSF_SAMPLE_COUNT       (WIFI_TSF_SAMPLE_PERIOD_MS*10)

class PrecisionTimeTask : public TaskBase {
public:
    const char* TAG = "TSF";
    PrecisionTimeTask() {

    }
    
    uint64_t GetTime_ns() {
        uint64_t timer_count = 0;
        gptimer_get_raw_count(hr_timer, &timer_count);
        //uint64_t real_time = static_cast<uint64_t>((_slope / 100'000.0) * static_cast<double>(timer_count)*25.0 + _offset*1000.0);
        //return static_cast<uint64_t>(real_time);
        return static_cast<uint64_t>(timer_count);
    }

    virtual void Run() override {
        high_resolution_timer_init();
        TickType_t timestamp = 0;
        while (1) {
            for(size_t sample = 0; sample < WIFI_TSF_SAMPLE_COUNT; sample++){
                xTaskDelayUntil(&timestamp, pdMS_TO_TICKS(100));
                int64_t tsf = esp_mesh_get_tsf_time();
                if(tsf == 0){
                    sample--;
                    continue;
                }
                _buff[sample] = tsf;
            }
            vPortEnterCritical();
            GetLinearRegression(_slope, _offset);
            reset_high_resolution_timer();
            vPortExitCritical();
            //ESP_LOGI(TAG, "New slope: %lf, offset: %lf", _slope, _offset);
        }
    }
private:
    void high_resolution_timer_init(void) {
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 40000000,  // 40MHz → 1 tick = 25ns
        };
        gptimer_new_timer(&timer_config, &hr_timer);
        gptimer_enable(hr_timer);
        gptimer_start(hr_timer);
    }

    void reset_high_resolution_timer(void) {
        gptimer_stop(hr_timer);
        gptimer_set_raw_count(hr_timer, 0);
        gptimer_start(hr_timer);
    }

    void GetLinearRegression(double &slope, double &offset) const {
        constexpr std::size_t n = WIFI_TSF_SAMPLE_COUNT;
        if (n < 2) {
            slope = 0.0;
            offset = static_cast<double>(_buff[0]);
            return;
        }
        
        double sumX  = 0.0;
        double sumY  = 0.0;
        double sumXY = 0.0;
        double sumX2 = 0.0;
        
        for (std::size_t i = 0; i < n; ++i) {
            double x = static_cast<double>(i);
            double y = static_cast<double>(_buff[i]);
            sumX  += x;
            sumY  += y;
            sumXY += x * y;
            sumX2 += x * x;
        }
        
        double denominator = n * sumX2 - sumX * sumX;
        if (denominator == 0.0) {
            slope = 0.0;
            offset = static_cast<double>(_buff[0]);
            return;
        }

        slope  = (n * sumXY - sumX * sumY) / denominator;
        offset = (sumY - slope * sumX) / n;
    }
    gptimer_handle_t hr_timer = NULL;
    double _slope;
    double _offset;
    int64_t _buff[WIFI_TSF_SAMPLE_COUNT];
};

