#pragma once
#include "config.h"
#include "SystemStructs.hpp"
#include "esp_mesh.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "driver/gptimer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "freertos/queue.h"
#include "esp_timer.h"
#include <cmath>

#define WIFI_TSF_SAMPLE_COUNT       (WIFI_TSF_SAMPLE_PERIOD_MS*10)

class PrecisionTimeTask : public TaskBase {
public:
    typedef struct{
        uint64_t tsf_ms;
        uint64_t timer_val;
    } TimingPoint_t;

    typedef struct {
        const TimingPoint_t* data;      // Указатель на массив данных
        size_t count;                   // Число элементов в массиве
        double slope;                  // Указатель на переменную для наклона
        double offset;                 // Указатель на переменную для смещения
        SemaphoreHandle_t doneSemaphore;// Семафор для синхронизации (сигнал завершения)
    } TheilTaskParams;


    const char* TAG = "TSF";
    PrecisionTimeTask() {

    }
    
    inline uint64_t GetTimeHrtim( void ){
        uint64_t timer_count = 0;
        if(hr_timer == NULL){
            return 0;
        }
        gptimer_get_raw_count(hr_timer, &timer_count);
        return timer_count;
    }

    uint64_t GetTime_ns() {
        uint64_t timer_count = GetTimeHrtim();
        return ConvertHrtimToNs(timer_count);
    }

    /*
    uint64_t ConvertHrtimToNs( uint64_t timer_count ){
        timer_count -= _timer_offset;
        uint64_t real_time = static_cast<uint64_t>(((static_cast<double>(timer_count) / _slope + _offset)*1'000.0));
        real_time += _tsf_offset*1000LLU;
        return static_cast<uint64_t>(real_time);
    }*/

    uint64_t ConvertHrtimToNs(uint64_t timer_value) {
        timer_value -= _timer_offset;
        if (_slope == 0.0) {
            // Avoid division by zero.
            return 0;
        }
        double tsf = ((double)timer_value - _offset) / _slope;
        // Round the result to the nearest integer.
        tsf += _tsf_offset;
        tsf *= 1000.0;
        return static_cast<uint64_t>(std::round(tsf));
    }

    virtual void Run( void* arg ) override {
        high_resolution_timer_init();
        TickType_t timestamp = 0;
        while (1) {
            timestamp = xTaskGetTickCount();
            for(size_t sample = 0; sample < WIFI_TSF_SAMPLE_COUNT; sample++){
                xTaskDelayUntil(&timestamp, pdMS_TO_TICKS(100));
                int64_t tsf = esp_mesh_get_tsf_time();
                if(tsf == 0){
                    sample--;
                    continue;
                }
                _buff[sample].tsf_ms = tsf;
                _buff[sample].timer_val = GetTimeHrtim();
            }
            
            vPortEnterCritical();
            uint64_t tsf_offset = _buff[0].tsf_ms;
            uint64_t timer_offset = _buff[0].timer_val;
            vPortExitCritical();
            for(size_t sample = 0; sample < WIFI_TSF_SAMPLE_COUNT; sample++){
                _buff[sample].tsf_ms -= _tsf_offset;
                _buff[sample].timer_val -= _timer_offset;
            }


            //ESP_LOGI(TAG, "T1:%llu", esp_timer_get_time());
            SemaphoreHandle_t doneSemaphore = xSemaphoreCreateBinary();
            TheilTaskParams params;
            params.data = _buff;
            params.count = WIFI_TSF_SAMPLE_COUNT;
            params.doneSemaphore = doneSemaphore;
            xTaskCreate(theil_calc_task, "theil_calc", 4096, (void*)&params, tskIDLE_PRIORITY+1, NULL);
            xSemaphoreTake(doneSemaphore, portMAX_DELAY);
            vSemaphoreDelete(doneSemaphore);
            //ESP_LOGI(TAG, "T2:%llu", esp_timer_get_time());
            
            vPortEnterCritical();
            _slope = params.slope;
            _offset = params.offset;
            _tsf_offset = tsf_offset;
            _timer_offset = timer_offset;
            vPortExitCritical();
            ESP_LOGI(TAG, "New slope: %.8lf, offset: %lf", _slope, _offset);
        }
    }

    static void theil_calc_task( void* arg ){
        TheilTaskParams* params = (TheilTaskParams*) arg;
        theil_sen_calc(params->data, params->count, params->slope, params->offset);
        xSemaphoreGive(params->doneSemaphore);
        vTaskDelete(NULL);
    }
private:
    void high_resolution_timer_init(void) {
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 40'000'000,  // 40MHz → 1 tick = 25ns
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

    static void theil_sen_calc(const TimingPoint_t* data, size_t count, double &slope, double &offset) {
        bool found_first = false;
        double minSlope = 0.0, maxSlope = 0.0;
        uint64_t valid_pair_count = 0;
    
        // Определяем диапазон возможных наклонов по всем парам точек (если tsf_ms различаются)
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = i + 1; j < count; ++j) {
                if (data[j].tsf_ms != data[i].tsf_ms) {
                    double curSlope = (double)(data[j].timer_val - data[i].timer_val) /
                                      (double)(data[j].tsf_ms - data[i].tsf_ms);
                    if (!found_first) {
                        minSlope = curSlope;
                        maxSlope = curSlope;
                        found_first = true;
                    } else {
                        if (curSlope < minSlope) minSlope = curSlope;
                        if (curSlope > maxSlope) maxSlope = curSlope;
                    }
                    ++valid_pair_count;
                }
            }
        }
    
        // Если ни одной валидной пары не найдено, возвращаем нули
        if (!found_first || valid_pair_count == 0) {
            slope = 0.0;
            offset = 0.0;
            return;
        }
    
        // Положение медианы – половина числа валидных пар
        uint64_t target = valid_pair_count / 2;
        const double tol = 1e-9;
        double candidate = 0.0;
    
        // Бинарный поиск по диапазону [minSlope, maxSlope]
        while ((maxSlope - minSlope) > tol) {
            candidate = (minSlope + maxSlope) / 2.0;
            uint64_t countBelow = 0;
            for (size_t i = 0; i < count; ++i) {
                for (size_t j = i + 1; j < count; ++j) {
                    if (data[j].tsf_ms != data[i].tsf_ms) {
                        double curSlope = (double)(data[j].timer_val - data[i].timer_val) /
                                          (double)(data[j].tsf_ms - data[i].tsf_ms);
                        if (curSlope < candidate)
                            ++countBelow;
                    }
                }
            }
            if (countBelow < target)
                minSlope = candidate;
            else
                maxSlope = candidate;
        }
    
        double medianSlope = (minSlope + maxSlope) / 2.0;
        slope = medianSlope;
    
        // Вычисляем смещения для каждой точки: offset_i = timer_val - medianSlope * tsf_ms
        double offsets[WIFI_TSF_SAMPLE_COUNT];
        for (size_t i = 0; i < count; ++i) {
            offsets[i] = (double)(data[i].timer_val) - medianSlope * (double)(data[i].tsf_ms);
        }
        // Для небольшого количества точек используем сортировку пузырьком для нахождения медианы смещений
        for (size_t i = 0; i < count - 1; ++i) {
            for (size_t j = i + 1; j < count; ++j) {
                if (offsets[j] < offsets[i]) {
                    double tmp = offsets[i];
                    offsets[i] = offsets[j];
                    offsets[j] = tmp;
                }
            }
        }
        double medianOffset = offsets[count / 2];
        offset = medianOffset;
    }

    gptimer_handle_t hr_timer = NULL;
    double _slope;
    double _offset;
    uint64_t _tsf_offset;
    uint64_t _timer_offset;
    TimingPoint_t _buff[WIFI_TSF_SAMPLE_COUNT];
};

