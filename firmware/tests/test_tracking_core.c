#include "tracking_core.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static const char *a="11111111-1111-1111-1111-111111111111";
static const char *b="22222222-2222-2222-2222-222222222222";
int main(void) {
    tracking_config config={.pan={1000,2000,1500,1},.tilt={1200,1800,1500,-1},
        .gain_us=40,.max_step_us=20,.slew_us_per_s=100,.deadband=.04f};
    tracking_controller s; assert(tracking_config_valid(&config)); tracking_init(&s,&config,0);
    assert(tracking_observe(&s,a,1,true,.5f,.5f,0,0));
    assert(s.pan_target==1520 && s.tilt_target==1480);
    assert(!tracking_observe(&s,a,1,true,1,1,0,1));
    assert(!tracking_observe(&s,a,0,true,1,1,0,1));
    tracking_tick(&s,20); assert(s.pan_us==1502 && s.tilt_us==1498);
    assert(!tracking_observe(&s,a,2,true,NAN,0,0,30));
    assert(!tracking_observe(&s,a,2,true,0,0,501,30));
    assert(tracking_observe(&s,a,2,true,.01f,.01f,0,40));
    assert(s.pan_target==s.pan_us && s.tilt_target==s.tilt_us);
    tracking_tick(&s,541); float stopped=s.pan_us; tracking_tick(&s,561);
    assert(s.pan_us==stopped && !strcmp(tracking_state(&s,561),"stale"));
    assert(tracking_observe(&s,b,0,false,0,0,0,600));
    assert(!tracking_observe(&s,a,3,true,1,1,0,601));
    assert(s.pan_target==s.pan_us && s.tilt_target==s.tilt_us);
    for (unsigned i=1;i<400;i++) { assert(tracking_observe(&s,b,i,true,1,1,0,600+i*20)); tracking_tick(&s,600+i*20); }
    assert(s.pan_target==2000 && s.tilt_target==1200 && s.limited);
    assert(s.pan_us<=2000 && s.tilt_us>=1200);
    config.pan.min_us=config.pan.max_us=1500; assert(!tracking_config_valid(&config));
    tracking_init(&s,&config,0); assert(tracking_observe(&s,a,1,true,1,1,0,0));
    tracking_tick(&s,20); assert(s.pan_us==1500 && !strcmp(tracking_state(&s,20),"uncalibrated"));
    puts("tracking core: sequence, sessions, signs, deadband, limits, stale stop, uncalibrated output OK");
}
