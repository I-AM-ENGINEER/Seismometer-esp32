#pragma once
#include <cstdint>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

typedef enum{
    SOUND_FRAME_GAIN_1 = 0, // Real gain 1
    SOUND_FRAME_GAIN_10,    // Real gain 9.(009)
    SOUND_FRAME_GAIN_100,   // Real gain 100
    SOUND_FRAME_GAIN_1000,  // Real gain 1110
} sound_frame_gain_t;

typedef struct __attribute__((packed)){
    uint8_t* data;
    size_t samples_count;
    uint64_t timestamp;
    uint8_t channels_count;
    uint8_t bits_per_single_sample;
} I2S_to_DSP_package_t;

typedef struct __attribute__((packed)){
    int8_t channel_id;
    uint8_t frames_count;
    uint8_t bits_per_sample;
    uint16_t samples_per_frame;
    // Next should be SampleHeader_t * frames_count
} NetPackageHeader_t;

typedef struct __attribute__((packed)){
    uint64_t timespamp;
    float gain;
    // Next should be bytearray
} SampleHeader_t;

/*
typedef struct{
    int8_t channel_id;
    uint8_t frames_count;
    uint8_t bits_per_sample;
    uint16_t samples_per_frame;
    uint64_t timespamp;
    float gain;
    uint16_t data[samples_per_frame];
}*/

typedef struct{
    size_t package_size;
    uint8_t* data;
} NetQueueElement_t;


class TaskBase {
public:
    virtual ~TaskBase() {}
    virtual void Run( void* arg ) = 0;
    
    typedef struct{
        void* arg;
        TaskBase* task;
    } params_t;

    void Start( const char* name, configSTACK_DEPTH_TYPE stackSize, UBaseType_t priority, void* arg ) {
        _params.arg = arg;
        _params.task = this;
        xTaskCreate(&TaskBase::TaskEntry, name, stackSize, &_params, priority, &_handle);
    }
    
protected:
    TaskHandle_t _handle;
    
private:
    static void TaskEntry( void* pvParameters ) {
        params_t* params = static_cast<params_t*>(pvParameters);
        TaskBase* pTask = params->task;
        ESP_LOGE("SYS", "Task %s run", pcTaskGetName(pTask->_handle));
        pTask->Run(params->arg);
        ESP_LOGE("SYS", "Task %s delete", pcTaskGetName(pTask->_handle));
        vTaskDelete(nullptr);
    }
    params_t _params;
};

template<typename T>
class MessageQueue {
public:
    MessageQueue(UBaseType_t length) {
        _queue = xQueueCreate(length, sizeof(T));
        if(_queue == nullptr){
            ESP_LOGE("SYS", "Memory for MessageQueue not enought");
        }
    }
    
    ~MessageQueue() {
        if (_queue) {
            vQueueDelete(_queue);
        }
    }
    
    bool send(const T& data, TickType_t timeout = portMAX_DELAY) {
        return (xQueueSend(_queue, &data, timeout) == pdPASS);
    }
    
    bool receive(T& data, TickType_t timeout = portMAX_DELAY) {
        return (xQueueReceive(_queue, &data, timeout) == pdPASS);
    }
    
    QueueHandle_t getHandle() const { return _queue; }
    
private:
    QueueHandle_t _queue;
};

