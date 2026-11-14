#include "servo_manual_core.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void) {
    assert(servo_manual_valid(1500,1500,5000));
    assert(servo_manual_valid(1450,1550,20));
    assert(servo_manual_valid(1550,1450,5000));
    assert(!servo_manual_valid(1449,1500,1000));
    assert(!servo_manual_valid(1500,1551,1000));
    assert(!servo_manual_valid(500,2500,1000));
    assert(!servo_manual_valid(INT_MAX,INT_MIN,1000));
    assert(!servo_manual_valid(1500,1500,19));
    assert(!servo_manual_valid(1500,1500,5001));
    puts("manual servo: neutral, limits, extreme values and timeout bounds OK");
}
