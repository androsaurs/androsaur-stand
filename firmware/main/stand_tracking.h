#pragma once
#include <stdbool.h>
#include "cJSON.h"
void stand_tracking_init(void);
bool stand_tracking_observe(const cJSON *packet);
cJSON *stand_tracking_status(void);
