#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CHARGER_MANUAL_MAX_MS 120000
typedef struct { bool active, enabled; int64_t until_ms; } charger_override;
bool charger_override_set(charger_override *state, bool enabled, int duration_ms, int64_t now_ms);
bool charger_override_apply(charger_override *state, bool automatic_enabled, bool policy_allow, int64_t now_ms);
