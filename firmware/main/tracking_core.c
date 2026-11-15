#include "tracking_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool axis_valid(tracking_axis_config a) {
    return isfinite(a.min_us) && isfinite(a.max_us) && isfinite(a.neutral_us) &&
        a.min_us >= 500 && a.max_us <= 2500 && a.min_us < a.max_us &&
        a.neutral_us >= a.min_us && a.neutral_us <= a.max_us &&
        (a.direction == 1 || a.direction == -1);
}
bool tracking_config_valid(const tracking_config *c) {
    return axis_valid(c->pan) && axis_valid(c->tilt) &&
        isfinite(c->max_step_us) && c->max_step_us > 0 && isfinite(c->slew_us_per_s) && c->slew_us_per_s > 0 &&
        isfinite(c->deadband) && c->deadband >= 0 && c->deadband < 1 &&
        isfinite(c->smoothing_ms) && c->smoothing_ms >= 0 && c->smoothing_ms <= 1000 &&
        isfinite(c->fov_pan_deg) && c->fov_pan_deg > 0 && c->fov_pan_deg <= 90 &&
        isfinite(c->fov_tilt_deg) && c->fov_tilt_deg > 0 && c->fov_tilt_deg <= 90;
}
void tracking_init(tracking_controller *s, const tracking_config *c, int64_t now) {
    memset(s, 0, sizeof(*s)); s->config = *c;
    s->pan_us = s->pan_target = c->pan.neutral_us;
    s->tilt_us = s->tilt_target = c->tilt.neutral_us;
    s->last_tick_ms = now;
}
void tracking_reset_motion(tracking_controller *s) {
    s->pan_velocity = s->tilt_velocity = 0; s->pan_moving = s->tilt_moving = false;
    s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
    s->hist_count = s->hist_next = 0; s->limited = false;
}
static float clamp(float v, float low, float high) { return fmaxf(low, fminf(high, v)); }

// Servo position at the moment the camera frame was captured: the observed offset is
// relative to that pose, not to where the stand is now.
static void pose_at(const tracking_controller *s, int64_t when, float *pan, float *tilt) {
    *pan = s->pan_us; *tilt = s->tilt_us;
    int64_t best = INT64_MAX;
    for (unsigned i = 0; i < s->hist_count; i++) {
        int64_t d = llabs(s->hist_ms[i] - when);
        if (d < best) { best = d; *pan = s->hist_pan[i]; *tilt = s->hist_tilt[i]; }
    }
}
bool tracking_observe(tracking_controller *s, const char *session, uint64_t seq,
                      bool visible, float x, float y, int age, int64_t now) {
    if (!session || strlen(session) != 36 || age < 0 || age > 500 ||
        (visible && (!isfinite(x) || !isfinite(y) || fabsf(x) > 1 || fabsf(y) > 1))) return false;
    if (strcmp(session, s->session)) {
        for (unsigned i = 0; i < 8; i++) if (!strcmp(session, s->retired[i])) return false;
        if (s->session[0]) strcpy(s->retired[s->retired_next++ % 8], s->session);
        strcpy(s->session, session); s->received = false;
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us;
    }
    if (s->received && seq <= s->seq) return false;
    s->received = true; s->seq = seq; s->received_ms = now; s->age_ms = age;
    s->visible = visible; s->x = visible ? x : 0; s->y = visible ? y : 0;
    if (visible) {
        float pan, tilt; pose_at(s, now - age, &pan, &tilt);
        s->pan_target = pan + s->config.pan.direction * x * TRACKING_CORRECTION_GAIN * s->config.fov_pan_deg * TRACKING_US_PER_DEG;
        s->tilt_target = tilt + s->config.tilt.direction * y * TRACKING_CORRECTION_GAIN * s->config.fov_tilt_deg * TRACKING_US_PER_DEG;
    } else {
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us; s->limited = false;
    }
    return true;
}

// Speed grows with distance, brakes smoothly before the target and ramps at a bounded
// acceleration, so starts and stops never jump. The target is absolute, so camera latency
// and uneven frame times do not turn into overshoot.
static float move_axis(tracking_controller *s, float current, float target, tracking_axis_config axis,
                       float fov_deg, float *velocity, bool *moving, bool active, float dt) {
    const float vmax = s->config.slew_us_per_s;
    const float vmin = fminf(vmax / 2, 5 * TRACKING_US_PER_DEG);
    const float accel = vmax / fmaxf(s->config.smoothing_ms / 1000, 0.02f);
    const float dead = s->config.deadband * fov_deg * TRACKING_US_PER_DEG;
    float want = 0;
    if (active) {
        float goal = clamp(target, axis.min_us, axis.max_us);
        float err = goal - current, margin = *moving ? dead * .5f : dead;
        if (fabsf(err) <= margin) *moving = false;
        else {
            *moving = true;
            float braking = sqrtf(accel * (fabsf(err) - margin));
            want = copysignf(fmaxf(fminf(vmax, braking), vmin), err);
        }
        if (target != goal && fabsf(err) <= margin) s->limited = true;
    } else *moving = false;
    float change = clamp(want - *velocity, -accel * dt, accel * dt);
    *velocity += change;
    float next = current + *velocity * dt;
    float bounded = clamp(next, axis.min_us, axis.max_us);
    if (bounded != next) { s->limited = true; *velocity = 0; }
    // When narrowing limits, return into the new envelope at the configured speed.
    if (current < axis.min_us) bounded = fminf(axis.min_us, current + vmax * dt);
    if (current > axis.max_us) bounded = fmaxf(axis.max_us, current - vmax * dt);
    return bounded;
}
void tracking_tick(tracking_controller *s, int64_t now) {
    float dt = clamp((float)(now - s->last_tick_ms), 0, 40) / 1000;
    s->last_tick_ms = now;
    if (!tracking_config_valid(&s->config)) return;
    if (dt <= 0) return;
    bool active = s->received && s->visible && now - s->received_ms + s->age_ms <= 500;
    if (!active) { s->limited = false; s->pan_target = s->pan_us; s->tilt_target = s->tilt_us; }
    else s->limited = false;
    s->pan_us = move_axis(s, s->pan_us, s->pan_target, s->config.pan, s->config.fov_pan_deg,
                          &s->pan_velocity, &s->pan_moving, active, dt);
    s->tilt_us = move_axis(s, s->tilt_us, s->tilt_target, s->config.tilt, s->config.fov_tilt_deg,
                           &s->tilt_velocity, &s->tilt_moving, active, dt);
    unsigned slot = s->hist_next++ % TRACKING_HISTORY;
    s->hist_ms[slot] = now; s->hist_pan[slot] = s->pan_us; s->hist_tilt[slot] = s->tilt_us;
    if (s->hist_count < TRACKING_HISTORY) s->hist_count++;
}
const char *tracking_state(const tracking_controller *s, int64_t now) {
    if (!tracking_config_valid(&s->config)) return "uncalibrated";
    if (!s->received) return "waiting";
    if (now - s->received_ms + s->age_ms > 500) return "stale";
    if (!s->visible) return "no_target";
    return s->limited ? "limited" : "tracking";
}
