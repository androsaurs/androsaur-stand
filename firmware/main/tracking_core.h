#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TRACKING_HISTORY 64
// Apply 85% of each measured offset: a safety margin against error in field of view and latency.
#define TRACKING_CORRECTION_GAIN 0.85f
#define TRACKING_US_PER_DEG (2000.0f / 180.0f)

typedef struct {
    float min_us, max_us, neutral_us, direction;
} tracking_axis_config;
typedef struct {
    tracking_axis_config pan, tilt;
    float max_step_us, slew_us_per_s, deadband, smoothing_ms, fov_pan_deg, fov_tilt_deg;
} tracking_config;
typedef struct {
    tracking_config config;
    float pan_us, tilt_us, pan_target, tilt_target, x, y;
    float pan_velocity, tilt_velocity;
    bool pan_moving, tilt_moving;
    bool visible, limited, received;
    int64_t hist_ms[TRACKING_HISTORY];
    float hist_pan[TRACKING_HISTORY], hist_tilt[TRACKING_HISTORY];
    unsigned hist_count, hist_next;
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
void tracking_reset_motion(tracking_controller *state);
const char *tracking_state(const tracking_controller *state, int64_t now_ms);
