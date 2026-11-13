#include "battery_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *a="00000000-0000-0000-0000-000000000001", *b="00000000-0000-0000-0000-000000000002";
int main(void) {
    battery_controller s; battery_init(&s,40,80);
    assert(s.allow && !s.received);
    assert(battery_observe(&s,a,1,60,false,0,100)); assert(s.allow && !s.charging);
    assert(battery_observe(&s,a,2,80,true,0,200)); assert(!s.allow && s.charging);
    assert(battery_observe(&s,a,3,60,false,0,300)); assert(!s.allow);
    assert(!battery_observe(&s,a,2,0,false,0,400)); assert(!s.allow && s.percent==60);
    assert(battery_observe(&s,b,0,60,false,0,400)); assert(!s.allow); // App restart keeps hysteresis.
    assert(!battery_observe(&s,a,4,0,false,0,500)); assert(!s.allow);
    assert(battery_observe(&s,b,1,40,false,0,500)); assert(s.allow);
    assert(battery_observe(&s,b,2,79,true,0,600)); assert(s.allow);
    assert(battery_observe(&s,b,3,100,false,0,700)); assert(!s.allow);
    battery_tick(&s,600700); assert(!s.allow);
    battery_tick(&s,600701); assert(s.allow); // Lost phone enables power after 10 min.
    assert(battery_observe(&s,b,4,90,false,60000,700000)); assert(!s.allow);
    battery_tick(&s,1240001); assert(s.allow); // Packet age counts toward watchdog.
    battery_controller before=s;
    assert(!battery_observe(&s,b,5,-1,false,0,1300000));
    assert(!battery_observe(&s,b,5,101,true,0,1300000));
    assert(!battery_observe(&s,b,5,50,true,60001,1300000));
    assert(!battery_observe(&s,"bad",5,50,true,0,1300000));
    assert(!memcmp(&s,&before,sizeof(s)));
    battery_init(&s,80,40); assert(!battery_observe(&s,a,1,50,false,0,0));
    puts("battery core: hysteresis, charging fact, replay/session rejection, invalid data, watchdog and age OK");
}
