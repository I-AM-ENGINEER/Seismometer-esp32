#ifndef I2S_TASK_H__
#define I2S_TASK_H__

#include <stdint.h>
#include "config.h"

typedef struct {
    uint64_t timestamp;
    uint8_t data[I2S_DMA_BUFFER_SIZE * I2S_DMA_FRAMES_BUFFER];
} i2s_dma_package_t;

void i2s_task( void *arg );

#endif // !I2S_TASK_H__
