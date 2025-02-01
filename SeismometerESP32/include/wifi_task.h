#ifndef WIFI_TASK_H__
#define WIFI_TASK_H__
#include <stdint.h>
#include "config.h"

typedef struct __attribute__((packed)) {
    uint8_t data[TCP_PACKAGE_LENGTH];
} tcp_packet_t;


void wifi_init_sta( void );
void tcp_send_task( void *pvParameters );

#endif
