#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float min_us, max_us, neutral_us, direction;
} tracking_axis_config;
typedef struct {
    tracking_axis_config pan, tilt;
    float gain_us, max_step_us, slew_us_per_s, deadband;
} tracking_config;
typedef struct {
    tracking_config config;
    float pan_us, tilt_us, pan_target, tilt_target, x, y;
    bool visible, limited, received;
    int64_t received_ms, last_tick_ms;
    int age_ms;
    uint64_t seq;
    char session[37], retired[8][37];
    unsigned retired_next;
} tracking_controller;

bool tracking_config_valid(const tracking_config *config);
void tracking_init(tracking_controller *state, const tracking_config *config, int64_t now_ms);
bool tracking_observe(tracking_controller *state, const char *session, uint64_t seq,
                      bool visible, float x, float y, int age_ms, int64_t now_ms);
void tracking_tick(tracking_controller *state, int64_t now_ms);
const char *tracking_state(const tracking_controller *state, int64_t now_ms);
