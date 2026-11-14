#include "tracking_core.h"
#include <math.h>
#include <string.h>

static bool axis_valid(tracking_axis_config a) {
    return isfinite(a.min_us) && isfinite(a.max_us) && isfinite(a.neutral_us) &&
        a.min_us >= 500 && a.max_us <= 2500 && a.min_us < a.max_us &&
        a.neutral_us >= a.min_us && a.neutral_us <= a.max_us &&
        (a.direction == 1 || a.direction == -1);
}
bool tracking_config_valid(const tracking_config *c) {
    return axis_valid(c->pan) && axis_valid(c->tilt) && isfinite(c->gain_us) && c->gain_us > 0 &&
        isfinite(c->max_step_us) && c->max_step_us > 0 && isfinite(c->slew_us_per_s) && c->slew_us_per_s > 0 &&
        isfinite(c->deadband) && c->deadband >= 0 && c->deadband < 1 &&
        isfinite(c->smoothing_ms) && c->smoothing_ms >= 0 && c->smoothing_ms <= 1000;
}
void tracking_init(tracking_controller *s, const tracking_config *c, int64_t now) {
    memset(s, 0, sizeof(*s)); s->config = *c;
    s->pan_us = s->pan_target = c->pan.neutral_us;
    s->tilt_us = s->tilt_target = c->tilt.neutral_us;
    s->last_tick_ms = now;
}
static float clamp(float v, float low, float high) { return fmaxf(low, fminf(high, v)); }
bool tracking_observe(tracking_controller *s, const char *session, uint64_t seq,
                      bool visible, float x, float y, int age, int64_t now) {
    if (!session || strlen(session) != 36 || age < 0 || age > 500 ||
        (visible && (!isfinite(x) || !isfinite(y) || fabsf(x) > 1 || fabsf(y) > 1))) return false;
    if (strcmp(session, s->session)) {
        for (unsigned i = 0; i < 8; i++) if (!strcmp(session, s->retired[i])) return false;
        if (s->session[0]) strcpy(s->retired[s->retired_next++ % 8], s->session);
        strcpy(s->session, session); s->received = false; s->filter_ready = false;
        s->pan_velocity = s->tilt_velocity = 0;
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
    }
    if (s->received && seq <= s->seq) return false;
    s->received = true; s->seq = seq; s->received_ms = now; s->age_ms = age;
    s->visible = visible; s->x = visible ? x : 0; s->y = visible ? y : 0;
    if (!visible) {
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
        s->pan_velocity = s->tilt_velocity = 0; s->filter_ready = false; s->limited = false;
    }
    return true;
}
static float approach(float current, float target, float limit) { return current + clamp(target - current, -limit, limit); }
static float move_axis(tracking_controller *s, float current, float error,
                       tracking_axis_config axis, float *velocity, float dt) {
    // Fixed-time control: the correction step is per 20 ms, independent of camera FPS.
    float corrected = copysignf(fmaxf(0, fabsf(error) - s->config.deadband), error);
    float desired = clamp(s->config.gain_us * corrected * axis.direction,
                          -s->config.max_step_us, s->config.max_step_us) / .02f;
    desired = clamp(desired, -s->config.slew_us_per_s, s->config.slew_us_per_s);
    float tau = s->config.smoothing_ms / 1000;
    *velocity = tau > 0 ? approach(*velocity, desired, s->config.slew_us_per_s * dt / tau) : desired;
    float next = current + *velocity * dt;
    float bounded = clamp(next, axis.min_us, axis.max_us);
    if (bounded != next) { s->limited = true; *velocity = 0; }
    // When narrowing limits, return into the new envelope at the configured speed.
    if (current < axis.min_us) bounded = fminf(axis.min_us, current + s->config.slew_us_per_s * dt);
    if (current > axis.max_us) bounded = fmaxf(axis.max_us, current - s->config.slew_us_per_s * dt);
    return bounded;
}
void tracking_tick(tracking_controller *s, int64_t now) {
    float dt = clamp((float)(now - s->last_tick_ms), 0, 40) / 1000;
    s->last_tick_ms = now;
    if (!s->received || !s->visible || now - s->received_ms + s->age_ms > 500) {
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
        s->pan_velocity = s->tilt_velocity = 0; s->filter_ready = false; s->limited = false; return;
    }
    if (!tracking_config_valid(&s->config)) return;
    if (dt <= 0) return;
    float alpha = s->config.smoothing_ms > 0 ? 1 - expf(-dt * 1000 / s->config.smoothing_ms) : 1;
    if (!s->filter_ready) { s->filtered_x = s->x; s->filtered_y = s->y; s->filter_ready = true; }
    s->filtered_x += alpha * (s->x - s->filtered_x);
    s->filtered_y += alpha * (s->y - s->filtered_y);
    s->limited = false;
    s->pan_us = move_axis(s, s->pan_us, s->filtered_x, s->config.pan, &s->pan_velocity, dt);
    s->tilt_us = move_axis(s, s->tilt_us, s->filtered_y, s->config.tilt, &s->tilt_velocity, dt);
    s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
}
const char *tracking_state(const tracking_controller *s, int64_t now) {
    if (!tracking_config_valid(&s->config)) return "uncalibrated";
    if (!s->received) return "waiting";
    if (now - s->received_ms + s->age_ms > 500) return "stale";
    if (!s->visible) return "no_target";
    return s->limited ? "limited" : "tracking";
}
