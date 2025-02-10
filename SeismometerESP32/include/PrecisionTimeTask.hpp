#pragma once

#include "SystemStructs.hpp"
#include "esp_mesh.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/gptimer.h"
//#include "ringbuffer.hpp"
#include <array>
#include <cstddef>

template <typename T, std::size_t Size>
class CircularBuffer {
public:
    // Add a new value into the circular buffer
    void push(T value) {
        head = (head + 1) % Size;
        if (full) {
            tail = (tail + 1) % Size;
        }
        buffer[head] = value;
        full = head == tail;
    }

    // Return the element 'count' positions from the newest.
    // If count is out of range (i.e. >= current size), returns the oldest element.
    T read(std::size_t count) const {
        if (count >= size()) {
            return buffer[tail]; // Return oldest available if count is out of range
        }
        std::size_t index = (head + Size - count) % Size;
        return buffer[index];
    }

    // Returns the number of valid elements currently in the buffer.
    std::size_t size() const {
        if (full) return Size;
        return (head >= tail) ? (head - tail + 1) : (Size - tail + head + 1);
    }

private:
    std::array<T, Size> buffer{};
    std::size_t head = Size - 1;
    std::size_t tail = 0;
    bool full = false;
};


class PrecisionTimeTask : public TaskBase {
public:
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

    const char* TAG = "TSF";
    PrecisionTimeTask() {

    }
    
    virtual void Run() override {
        high_resolution_timer_init();
        //ESP_LOGI(TAG, "Starting TSF sync task");
        
        //sync_frame_t samples[TFS_SYNC_ARRAY_COUNT];
        //int sample_index = 0;
        TickType_t timestamp = 0;
        while (1) {
            static int64_t old_tsf;
            static uint8_t tsf_count;
            
            int64_t tsf = esp_mesh_get_tsf_time();
            tsf_count++;

            if (tsf == 0) {
                ESP_LOGE(TAG, "TSF miss");
                continue;
            }
            uint32_t delta = (tsf - old_tsf)/tsf_count;
            if(old_tsf == 0){
                old_tsf = tsf;
                continue;
            }
            old_tsf = tsf;
            tsf_count = 0;

            
            
            _circ_buff.push(delta);
            printf("%f\n", 4000000.0f/GetLinearRegression());
            
            xTaskDelayUntil(&timestamp, pdMS_TO_TICKS(100));
        }
        //vTaskDelay(pdMS_TO_TICKS(1000));
    }

    float GetLinearRegression(void) {
        std::size_t n = _circ_buff.size();
        if (n < 2) {
            return _circ_buff.read(0);
        }

        int64_t sumX = 0.0;
        int64_t sumY = 0.0;
        int64_t sumXY = 0.0;
        int64_t sumX2 = 0.0;

        for (std::size_t i = 0; i < n; i++) {
            int64_t y = static_cast<int64_t>(_circ_buff.read(n - 1 - i));
            int64_t x = static_cast<int64_t>(i);
            
            sumX  += x;
            sumY  += y;
            sumXY += x * y;
            sumX2 += x * x;
        }

        double denominator = (double)((int64_t)n * sumX2 - sumX * sumX);
        if (denominator == 0.0) {
            return _circ_buff.read(0);
        }

        double slope = (double)((int64_t)n * sumXY - sumX * sumY) / denominator;
        double intercept = ((double)sumY - slope * (double)sumX) / (double)n;

        float predicted = static_cast<float>(slope * (double)n + intercept);
        return predicted;
    }
private:
    gptimer_handle_t hr_timer = NULL;
    CircularBuffer<int32_t, 1200> _circ_buff;
    //int64_t _buff[300];
};

