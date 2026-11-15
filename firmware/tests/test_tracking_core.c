#include "tracking_core.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#define UPD TRACKING_US_PER_DEG
static const char *a="11111111-1111-1111-1111-111111111111";
static const char *b="22222222-2222-2222-2222-222222222222";
static tracking_config config(void) {
    return (tracking_config){.pan={1000,2000,1500,1},.tilt={1200,1800,1500,-1},
        .max_step_us=20,.slew_us_per_s=30*UPD,.deadband=.04f,.smoothing_ms=200,
        .fov_pan_deg=30,.fov_tilt_deg=30};
}
static float accel_limit(const tracking_config *c) { return c->slew_us_per_s/(c->smoothing_ms/1000); }

// Closed loop: a fixed face at face_deg, camera frames every `period` ms after `latency` ms.
typedef struct { float pan_deg, max_speed, max_dv, overshoot; int64_t reached_ms; } loop_result;
static loop_result closed_loop(float face_deg, int period, int latency) {
    tracking_config cfg=config(); tracking_controller s; tracking_init(&s,&cfg,0);
    loop_result r={0,0,0,0,-1};
    struct { int64_t due; float x; } queue[64]; unsigned head=0,tail=0; unsigned seq=0;
    float prev_v=0;
    for (int t=0;t<=6000;t+=20) {
        float pan_deg=(s.pan_us-1500)/UPD;
        if (t%period==0) {
            queue[tail%64].due=t+latency; queue[tail%64].x=fmaxf(-1,fminf(1,(face_deg-pan_deg)/cfg.fov_pan_deg)); tail++;
        }
        while (head<tail && queue[head%64].due<=t) {
            assert(tracking_observe(&s,a,++seq,true,queue[head%64].x,0,latency,t)); head++;
        }
        tracking_tick(&s,t);
        float v=s.pan_velocity;
        if (fabsf(v)>r.max_speed) r.max_speed=fabsf(v);
        if (fabsf(v-prev_v)>r.max_dv) r.max_dv=fabsf(v-prev_v);
        prev_v=v;
        float pd=(s.pan_us-1500)/UPD;
        if (pd>face_deg && pd-face_deg>r.overshoot) r.overshoot=pd-face_deg;
        if (r.reached_ms<0 && fabsf(pd-face_deg)<1.5f) r.reached_ms=t;
    }
    r.pan_deg=(s.pan_us-1500)/UPD; return r;
}
int main(void) {
    tracking_config cfg=config(); tracking_controller s;
    assert(tracking_config_valid(&cfg)); tracking_init(&s,&cfg,0);
    assert(tracking_observe(&s,a,1,true,.5f,.5f,0,0));
    assert(!tracking_observe(&s,a,1,true,1,1,0,1));
    assert(!tracking_observe(&s,a,0,true,1,1,0,1));
    assert(!tracking_observe(&s,a,2,true,NAN,0,0,30));
    assert(!tracking_observe(&s,a,2,true,0,0,501,30));
    tracking_tick(&s,20); assert(s.pan_us>1500 && s.tilt_us<1500);          // signs: pan +, tilt reversed
    assert(fabsf(s.pan_target-(1500+.5f*TRACKING_CORRECTION_GAIN*30*UPD))<.01f);                       // absolute target from the offset

    // Smooth start and stop: speed never jumps by more than the acceleration limit per tick.
    float limit=accel_limit(&cfg)*.02f+.001f;
    loop_result r=closed_loop(24,150,120);
    assert(r.max_dv<=limit && r.max_speed<=cfg.slew_us_per_s+.01f);
    assert(r.reached_ms>=0 && r.reached_ms<3000);                              // arrives quickly, not at 2.5 deg/s
    assert(r.overshoot<1.5f && fabsf(r.pan_deg-24)<1.5f);                      // no oscillation around the target
    // Camera latency and uneven frame period must not change where it settles.
    loop_result slow=closed_loop(24,400,250);
    assert(slow.overshoot<2.5f && fabsf(slow.pan_deg-24)<1.5f);
    assert(fabsf(closed_loop(-20,100,100).pan_deg+20)<1.5f);

    // The offset is relative to the pose when the frame was captured, not the pose now.
    tracking_init(&s,&cfg,0);
    assert(tracking_observe(&s,a,1,true,.8f,0,0,0));
    for (int t=20;t<=300;t+=20) tracking_tick(&s,t);
    float moved=s.pan_us;
    assert(tracking_observe(&s,a,2,true,.8f,0,260,300));                       // frame captured at t=40 (old pose)
    assert(s.pan_target<moved+.8f*30*UPD && s.pan_target>1500+.8f*TRACKING_CORRECTION_GAIN*30*UPD-1);   // target anchored to the capture pose

    // Lost or stale target: decelerate smoothly instead of jumping, then hold still.
    tracking_init(&s,&cfg,0);
    for (int t=0;t<=600;t+=20) { assert(tracking_observe(&s,a,t/20+1,true,1,0,0,t)); tracking_tick(&s,t); }
    float before=s.pan_velocity; assert(before>0);
    tracking_tick(&s,1100); tracking_tick(&s,1120);                            // frames stopped >500 ms ago
    assert(!strcmp(tracking_state(&s,1120),"stale"));
    assert(s.pan_velocity<before && s.pan_velocity>=before-limit);
    for (int t=1140;t<=2000;t+=20) tracking_tick(&s,t);
    float stopped=s.pan_us; assert(s.pan_velocity==0);
    tracking_tick(&s,2020); assert(s.pan_us==stopped);

    // Sessions: a new session resets the target; a retired one is rejected.
    assert(tracking_observe(&s,b,0,false,0,0,0,2100));
    assert(!tracking_observe(&s,a,99,true,1,1,0,2101));
    assert(s.pan_target==s.pan_us && !strcmp(tracking_state(&s,2100),"no_target"));
    tracking_tick(&s,2120); assert(s.pan_us==stopped);

    // Limits: it reaches the end of the range, reports limited and never leaves it.
    for (unsigned i=1;i<400;i++) {
        assert(tracking_observe(&s,b,i,true,1,1,0,2200+i*20)); tracking_tick(&s,2200+i*20);
        assert(s.pan_us<=2000 && s.tilt_us>=1200);
    }
    assert(fabsf(s.pan_us-1500-30*UPD*1)<=500 && s.limited);
    // Narrowing the range returns into it gradually at the configured speed.
    s.config.pan.max_us=1600; before=s.pan_us; tracking_tick(&s,s.last_tick_ms+20);
    assert(s.pan_us<before && s.pan_us>=before-cfg.slew_us_per_s*.02f-.01f);

    cfg.pan.min_us=cfg.pan.max_us=1500; assert(!tracking_config_valid(&cfg));
    tracking_init(&s,&cfg,0); assert(tracking_observe(&s,a,1,true,1,1,0,0));
    tracking_tick(&s,20); assert(s.pan_us==1500 && !strcmp(tracking_state(&s,20),"uncalibrated"));
    cfg=config(); cfg.smoothing_ms=NAN; assert(!tracking_config_valid(&cfg));
    cfg=config(); cfg.fov_pan_deg=0; assert(!tracking_config_valid(&cfg));
    puts("tracking: signs, smooth profile, latency compensation, no overshoot, stale decel, sessions, limits OK");
}
