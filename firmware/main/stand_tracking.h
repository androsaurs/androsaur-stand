#pragma once
#include <stdbool.h>
#include "cJSON.h"
void stand_tracking_init(void);
bool stand_tracking_observe(const cJSON *packet);
cJSON *stand_tracking_status(void);
bool stand_tracking_center_available(void);
bool stand_tracking_center(void);
bool stand_tracking_manual(const cJSON *packet);
bool stand_tracking_stop(void);
bool stand_tracking_configure(const cJSON *packet);
bool stand_tracking_enable(const cJSON *packet);
