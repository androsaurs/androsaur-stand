#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    char session[37], retired[8][37]; unsigned retired_next;
    uint64_t seq; int percent, age_ms, low, high; int64_t received_ms;
    bool received, charging, allow;
} battery_controller;
void battery_init(battery_controller *s, int low, int high);
bool battery_observe(battery_controller *s, const char *session, uint64_t seq, int percent, bool charging, int age, int64_t now);
void battery_tick(battery_controller *s, int64_t now);
