#include "servo_manual_core.h"
bool servo_manual_valid(int pan_us, int tilt_us, int duration_ms) {
    return pan_us>=SERVO_MANUAL_MIN_US && pan_us<=SERVO_MANUAL_MAX_US &&
           tilt_us>=SERVO_MANUAL_MIN_US && tilt_us<=SERVO_MANUAL_MAX_US &&
           duration_ms>=20 && duration_ms<=SERVO_MANUAL_MAX_DURATION_MS;
}
