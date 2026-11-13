#pragma once

#include <stdint.h>
#include "cJSON.h"

// The journal is intentionally small and event-based: no periodic heartbeat writes.
#define STAND_LOG_CAPACITY 128

typedef enum {
    STAND_LOG_BOOT = 1,
    STAND_LOG_WIFI_START,
    STAND_LOG_WIFI_DISCONNECTED,
    STAND_LOG_GOT_IP,
    STAND_LOG_HTTPS_READY,
} stand_log_event_t;

void stand_log_init(void);
void stand_log_event(stand_log_event_t event, int reason, int rssi, const char *detail);
cJSON *stand_log_json(void);
