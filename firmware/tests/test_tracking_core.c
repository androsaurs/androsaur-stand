#include "tracking_core.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static const char *a="11111111-1111-1111-1111-111111111111";
static const char *b="22222222-2222-2222-2222-222222222222";
static tracking_config config(void) {
    return (tracking_config){.pan={1000,2000,1500,1},.tilt={1200,1800,1500,-1},
        .gain_us=80,.max_step_us=20,.slew_us_per_s=100,.deadband=.04f,.smoothing_ms=200};
}
static float run(int camera_period) {
    tracking_config cfg=config(); tracking_controller s; tracking_init(&s,&cfg,0);
    unsigned seq=0;
    for (int t=0;t<=2000;t+=20) {
        if (t%camera_period==0) assert(tracking_observe(&s,a,++seq,true,.5f,.5f,0,t));
        float before=s.pan_us; tracking_tick(&s,t); assert(fabsf(s.pan_us-before)<=2.001f);
    }
    assert(s.pan_us>1650 && s.tilt_us<1350);
    return s.pan_us;
}
int main(void) {
    tracking_config cfg=config(); tracking_controller s;
    assert(tracking_config_valid(&cfg)); tracking_init(&s,&cfg,0);
    assert(tracking_observe(&s,a,1,true,.5f,.5f,0,0));
    assert(!tracking_observe(&s,a,1,true,1,1,0,1));
    assert(!tracking_observe(&s,a,0,true,1,1,0,1));
    tracking_tick(&s,20); assert(s.pan_us>1500 && s.pan_us<1501 && s.tilt_us<1500);
    float velocity=s.pan_velocity;
    tracking_tick(&s,40); assert(s.pan_velocity>velocity && s.pan_velocity<=100);
    assert(!tracking_observe(&s,a,2,true,NAN,0,0,30));
    assert(!tracking_observe(&s,a,2,true,0,0,501,30));
    assert(tracking_observe(&s,a,2,true,-1,-1,0,50));
    velocity=s.pan_velocity; tracking_tick(&s,60);
    assert(fabsf(s.pan_velocity-velocity)<=10.001f);
    tracking_tick(&s,551); float stopped=s.pan_us; tracking_tick(&s,571);
    assert(s.pan_us==stopped && s.pan_velocity==0 && !strcmp(tracking_state(&s,571),"stale"));
    assert(tracking_observe(&s,b,0,false,0,0,0,600));
    assert(!tracking_observe(&s,a,3,true,1,1,0,601));
    assert(s.pan_velocity==0 && !s.filter_ready);
    assert(tracking_observe(&s,b,1,true,.01f,-.01f,0,620));
    tracking_tick(&s,620); assert(s.pan_us==stopped);
    for (unsigned i=2;i<500;i++) {
        assert(tracking_observe(&s,b,i,true,1,1,0,620+i*20)); tracking_tick(&s,620+i*20);
    }
    assert(s.pan_us==2000 && s.tilt_us==1200 && s.limited);
    s.config.pan.max_us=1600; float before=s.pan_us; tracking_tick(&s,s.last_tick_ms+20);
    assert(s.pan_us>=before-2.001f && s.pan_us<before);
    assert(fabsf(run(20)-run(200))<.01f);
    cfg.pan.min_us=cfg.pan.max_us=1500; assert(!tracking_config_valid(&cfg));
    tracking_init(&s,&cfg,0); assert(tracking_observe(&s,a,1,true,1,1,0,0));
    tracking_tick(&s,20); assert(s.pan_us==1500 && !strcmp(tracking_state(&s,20),"uncalibrated"));
    cfg=config(); cfg.smoothing_ms=NAN; assert(!tracking_config_valid(&cfg));
    puts("tracking: signs, FPS independence, noise, acceleration, limits, slow narrowing, stale stop, sessions OK");
}
