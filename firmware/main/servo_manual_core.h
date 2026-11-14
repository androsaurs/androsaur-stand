#pragma once
#include <stdbool.h>

// Conservative uncalibrated service range around nominal neutral, not degrees.
#define SERVO_MANUAL_MIN_US 1450
#define SERVO_MANUAL_MAX_US 1550
#define SERVO_MANUAL_MAX_DURATION_MS 5000
bool servo_manual_valid(int pan_us, int tilt_us, int duration_ms);
