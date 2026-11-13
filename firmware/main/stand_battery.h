#pragma once
#include <stdbool.h>
#include "cJSON.h"
void stand_battery_init(void);
bool stand_battery_observe(const cJSON *packet);
cJSON *stand_battery_status(void);
