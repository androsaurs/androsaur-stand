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
        isfinite(c->deadband) && c->deadband >= 0 && c->deadband < 1;
}
void tracking_init(tracking_controller *s, const tracking_config *c, int64_t now) {
    memset(s, 0, sizeof(*s)); s->config = *c;
    s->pan_us = s->pan_target = c->pan.neutral_us;
    s->tilt_us = s->tilt_target = c->tilt.neutral_us;
    s->last_tick_ms = now;
}
static float clamp(float v, float low, float high) { return fmaxf(low, fminf(high, v)); }
static float step(tracking_controller *s, float current, float error, tracking_axis_config axis) {
    if (fabsf(error) <= s->config.deadband) return current;
    float delta = clamp(s->config.gain_us * error * axis.direction, -s->config.max_step_us, s->config.max_step_us);
    float target = clamp(current + delta, axis.min_us, axis.max_us);
    if (target != current + delta) s->limited = true;
    return target;
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
    s->visible = visible; s->x = visible ? x : 0; s->y = visible ? y : 0; s->limited = false;
    if (!visible) { s->pan_target = s->pan_us; s->tilt_target = s->tilt_us; return true; }
    if (tracking_config_valid(&s->config)) {
        s->pan_target = step(s, s->pan_us, x, s->config.pan);
        s->tilt_target = step(s, s->tilt_us, y, s->config.tilt);
    }
    return true;
}
static float approach(float current, float target, float limit) { return current + clamp(target - current, -limit, limit); }
void tracking_tick(tracking_controller *s, int64_t now) {
    float dt = clamp((float)(now - s->last_tick_ms), 0, 40) / 1000;
    s->last_tick_ms = now;
    if (!s->received || !s->visible || now - s->received_ms + s->age_ms > 500) {
        s->pan_target = s->pan_us; s->tilt_target = s->tilt_us; return;
    }
    if (!tracking_config_valid(&s->config)) return;
    float move = s->config.slew_us_per_s * dt;
    s->pan_us = approach(s->pan_us, s->pan_target, move);
    s->tilt_us = approach(s->tilt_us, s->tilt_target, move);
}
const char *tracking_state(const tracking_controller *s, int64_t now) {
    if (!tracking_config_valid(&s->config)) return "uncalibrated";
    if (!s->received) return "waiting";
    if (now - s->received_ms + s->age_ms > 500) return "stale";
    if (!s->visible) return "no_target";
    return s->limited ? "limited" : "tracking";
}
