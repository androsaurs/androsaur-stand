#include "stand_tracking.h"
#include "tracking_core.h"
#include "servo_manual_core.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "nvs.h"

// Nominal MG996R mapping only: no encoder or measured angle calibration.
#define US_PER_DEG (2000.0f / 180.0f)
static bool tracking_enabled;
static bool pwm_available;
static float setting_value(float value) { return roundf(value*10000)/10000; }
static cJSON *settings_json(const tracking_config *cfg, bool enabled) {
    cJSON *p=cJSON_CreateObject();
    cJSON_AddBoolToObject(p,"enabled",enabled);
    cJSON_AddNumberToObject(p,"panRangeDeg",setting_value((cfg->pan.max_us-cfg->pan.min_us)/2/US_PER_DEG));
    cJSON_AddNumberToObject(p,"tiltRangeDeg",setting_value((cfg->tilt.max_us-cfg->tilt.min_us)/2/US_PER_DEG));
    cJSON_AddNumberToObject(p,"speedDegS",setting_value(cfg->slew_us_per_s/US_PER_DEG));
    cJSON_AddNumberToObject(p,"stepDeg",setting_value(cfg->max_step_us/US_PER_DEG));
    cJSON_AddNumberToObject(p,"smoothingMs",setting_value(cfg->smoothing_ms));
    cJSON_AddNumberToObject(p,"deadZonePercent",setting_value(cfg->deadband*100));
    cJSON_AddBoolToObject(p,"panReverse",cfg->pan.direction<0);
    cJSON_AddBoolToObject(p,"tiltReverse",cfg->tilt.direction<0);
    return p;
}
static bool number(const cJSON *p,const char *name,double low,double high,float *out) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(p,name);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<low || v->valuedouble>high) return false;
    *out=(float)v->valuedouble; return true;
}
static bool parse_settings(const cJSON *p, tracking_config *cfg, bool *enabled) {
    float pan,tilt,speed,step,smooth,dead;
    const cJSON *on=cJSON_GetObjectItemCaseSensitive(p,"enabled"),
        *pr=cJSON_GetObjectItemCaseSensitive(p,"panReverse"), *tr=cJSON_GetObjectItemCaseSensitive(p,"tiltReverse");
    if (!cJSON_IsObject(p) || !cJSON_IsBool(on) || !cJSON_IsBool(pr) || !cJSON_IsBool(tr) ||
        !number(p,"panRangeDeg",.5f,90,&pan) || !number(p,"tiltRangeDeg",.5f,45,&tilt) ||
        !number(p,"speedDegS",1,90,&speed) || !number(p,"stepDeg",.1,10,&step) ||
        !number(p,"smoothingMs",0,1000,&smooth) || !number(p,"deadZonePercent",1,20,&dead)) return false;
    cfg->pan.min_us=cfg->pan.neutral_us-pan*US_PER_DEG; cfg->pan.max_us=cfg->pan.neutral_us+pan*US_PER_DEG;
    cfg->tilt.min_us=cfg->tilt.neutral_us-tilt*US_PER_DEG; cfg->tilt.max_us=cfg->tilt.neutral_us+tilt*US_PER_DEG;
    cfg->pan.direction=cJSON_IsTrue(pr)?-1:1; cfg->tilt.direction=cJSON_IsTrue(tr)?-1:1;
    cfg->slew_us_per_s=speed*US_PER_DEG; cfg->max_step_us=step*US_PER_DEG;
    cfg->gain_us=cfg->max_step_us*4; cfg->smoothing_ms=smooth; cfg->deadband=dead/100;
    *enabled=cJSON_IsTrue(on); return tracking_config_valid(cfg);
}
static bool save_settings(const tracking_config *cfg,bool enabled) {
    cJSON *p=settings_json(cfg,enabled); char *raw=cJSON_PrintUnformatted(p); cJSON_Delete(p);
    if (!raw) return false;
    nvs_handle_t store; esp_err_t err=nvs_open("stand_tracking",NVS_READWRITE,&store);
    if (err==ESP_OK) { err=nvs_set_str(store,"settings",raw); if (err==ESP_OK) err=nvs_commit(store); nvs_close(store); }
    free(raw); return err==ESP_OK;
}

static tracking_controller controller;
static SemaphoreHandle_t lock;
static bool output;
static int64_t center_until_ms;
static int64_t last_center_ms;
static int64_t last_manual_ms;
static int commanded_pan_us=1500, commanded_tilt_us=1500;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static bool gpio_valid(int pin) { return pin >= 1 && pin <= 21 && pin != 3 && pin != 19 && pin != 20; }
static uint32_t duty(float us) { return (uint32_t)lroundf(us * 16384 / 20000); }
static esp_err_t start_pwm(float pan_us, float tilt_us) {
    ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_14_BIT,
        .timer_num=LEDC_TIMER_0,.freq_hz=50,.clk_cfg=LEDC_AUTO_CLK};
    esp_err_t err=ledc_timer_config(&timer);
    if (err!=ESP_OK) return err;
    ledc_channel_config_t pan={.gpio_num=CONFIG_STAND_PAN_GPIO,.speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=duty(pan_us)};
    ledc_channel_config_t tilt=pan; tilt.gpio_num=CONFIG_STAND_TILT_GPIO; tilt.channel=LEDC_CHANNEL_1; tilt.duty=duty(tilt_us);
    err=ledc_channel_config(&pan);
    if (err==ESP_OK) err=ledc_channel_config(&tilt);
    if (err!=ESP_OK) {
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0);
    }
    return err;
}
static void loop(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        xSemaphoreTake(lock, portMAX_DELAY);
        if (tracking_enabled && !center_until_ms) tracking_tick(&controller, now_ms());
        if (center_until_ms && now_ms()>=center_until_ms) {
            ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
            ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0);
            center_until_ms=0; output=false;
            controller.last_tick_ms=now_ms();
            if (tracking_enabled && start_pwm(controller.pan_us,controller.tilt_us)==ESP_OK) output=true;
        }
        float pan = controller.pan_us, tilt = controller.tilt_us;
        if (output && !center_until_ms) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty(pan)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty(tilt)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        }
        xSemaphoreGive(lock);
    }
}
void stand_tracking_init(void) {
    tracking_config cfg = {
        .pan={CONFIG_STAND_PAN_MIN_US,CONFIG_STAND_PAN_MAX_US,CONFIG_STAND_PAN_NEUTRAL_US,1},
        .tilt={CONFIG_STAND_TILT_MIN_US,CONFIG_STAND_TILT_MAX_US,CONFIG_STAND_TILT_NEUTRAL_US,1},
        // First physical tracking run: deliberately gentle motion around the
        // experimentally confirmed neutral. Widen only after observing the
        // complete mechanism under load.
        .gain_us=20,.max_step_us=5,.slew_us_per_s=25,.deadband=.04f,.smoothing_ms=200
    };
#ifdef CONFIG_STAND_PAN_REVERSE
    cfg.pan.direction=-1;
#endif
#ifdef CONFIG_STAND_TILT_REVERSE
    cfg.tilt.direction=-1;
#endif
#ifdef CONFIG_STAND_SERVO_OUTPUT
    tracking_enabled=true;
#else
    tracking_enabled=false;
#endif
    pwm_available=gpio_valid(CONFIG_STAND_PAN_GPIO) && gpio_valid(CONFIG_STAND_TILT_GPIO) && CONFIG_STAND_PAN_GPIO!=CONFIG_STAND_TILT_GPIO;
    nvs_handle_t store;
    if (nvs_open("stand_tracking",NVS_READONLY,&store)==ESP_OK) {
        char raw[512]; size_t len=sizeof(raw);
        if (nvs_get_str(store,"settings",raw,&len)==ESP_OK) {
            cJSON *saved=cJSON_Parse(raw); tracking_config candidate=cfg; bool on=false;
            if (parse_settings(saved,&candidate,&on)) { cfg=candidate; tracking_enabled=on; }
            else tracking_enabled=false;
            cJSON_Delete(saved);
        }
        nvs_close(store);
    }
    tracking_init(&controller, &cfg, now_ms()); lock=xSemaphoreCreateMutex(); assert(lock);
    if (tracking_enabled && tracking_config_valid(&cfg) && pwm_available) {
        ESP_ERROR_CHECK(start_pwm(cfg.pan.neutral_us,cfg.tilt.neutral_us)); output=true;
    } else if (tracking_enabled) ESP_LOGW("tracking", "Servo output disabled: unconfirmed GPIO or calibration");
    xTaskCreate(loop,"stand_tracking",3072,NULL,5,NULL);
}
bool stand_tracking_center_available(void) {
#ifdef CONFIG_STAND_SERVO_MANUAL_CONTROL
    return gpio_valid(CONFIG_STAND_PAN_GPIO) && gpio_valid(CONFIG_STAND_TILT_GPIO) &&
           CONFIG_STAND_PAN_GPIO!=CONFIG_STAND_TILT_GPIO;
#else
    return false;
#endif
}
static bool manual_move(int pan_us,int tilt_us,int duration_ms) {
    if (!stand_tracking_center_available() || !servo_manual_valid(pan_us,tilt_us,duration_ms)) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=start_pwm(pan_us,tilt_us)==ESP_OK;
    if (ok) {
        last_manual_ms=now_ms(); center_until_ms=last_manual_ms+duration_ms; output=true;
        commanded_pan_us=pan_us; commanded_tilt_us=tilt_us;
        controller.pan_us=controller.pan_target=pan_us; controller.tilt_us=controller.tilt_target=tilt_us;
        controller.pan_velocity=controller.tilt_velocity=0; controller.filter_ready=false;
        if (pan_us==1500 && tilt_us==1500) last_center_ms=last_manual_ms;
    }
    else { output=false; center_until_ms=0; }
    xSemaphoreGive(lock);
    return ok;
}
bool stand_tracking_center(void) { return manual_move(1500,1500,5000); }
static bool integer(const cJSON *item,int low,int high) {
    return cJSON_IsNumber(item) && isfinite(item->valuedouble) && item->valuedouble>=low &&
           item->valuedouble<=high && floor(item->valuedouble)==item->valuedouble;
}
bool stand_tracking_manual(const cJSON *p) {
    const cJSON *pan=cJSON_GetObjectItemCaseSensitive(p,"panUs"),
        *tilt=cJSON_GetObjectItemCaseSensitive(p,"tiltUs"),
        *duration=cJSON_GetObjectItemCaseSensitive(p,"durationMs");
    if (!cJSON_IsObject(p) || !integer(pan,SERVO_MANUAL_MIN_US,SERVO_MANUAL_MAX_US) ||
        !integer(tilt,SERVO_MANUAL_MIN_US,SERVO_MANUAL_MAX_US) ||
        !integer(duration,20,SERVO_MANUAL_MAX_DURATION_MS)) return false;
    return manual_move(pan->valueint,tilt->valueint,duration->valueint);
}
bool stand_tracking_stop(void) {
    xSemaphoreTake(lock,portMAX_DELAY);
    if (output) {
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0);
    }
    output=false; center_until_ms=0; tracking_enabled=false;
    bool saved=save_settings(&controller.config,false);
    xSemaphoreGive(lock);
    return saved;
}
static bool configure_locked(const tracking_config *cfg,bool enabled) {
    if (enabled && !pwm_available) return false;
    if (!save_settings(cfg,enabled)) return false;
    controller.config=*cfg; controller.pan_velocity=controller.tilt_velocity=0;
    controller.filter_ready=false; controller.limited=false; controller.last_tick_ms=now_ms();
    center_until_ms=0; tracking_enabled=enabled;
    if (!enabled) {
        ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0); ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0); output=false;
    } else if (!output) output=start_pwm(controller.pan_us,controller.tilt_us)==ESP_OK;
    return !enabled || output;
}
bool stand_tracking_configure(const cJSON *p) {
    xSemaphoreTake(lock,portMAX_DELAY); tracking_config cfg=controller.config; bool enabled=false;
    bool ok=parse_settings(p,&cfg,&enabled) && configure_locked(&cfg,enabled);
    xSemaphoreGive(lock); return ok;
}
bool stand_tracking_enable(const cJSON *p) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(p,"enabled");
    if (!cJSON_IsBool(v)) return false;
    xSemaphoreTake(lock,portMAX_DELAY); bool ok=configure_locked(&controller.config,cJSON_IsTrue(v));
    xSemaphoreGive(lock); return ok;
}
bool stand_tracking_observe(const cJSON *p) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(p,"v"), *session=cJSON_GetObjectItemCaseSensitive(p,"session"),
        *seq=cJSON_GetObjectItemCaseSensitive(p,"seq"), *visible=cJSON_GetObjectItemCaseSensitive(p,"visible"),
        *x=cJSON_GetObjectItemCaseSensitive(p,"x"), *y=cJSON_GetObjectItemCaseSensitive(p,"y"),
        *age=cJSON_GetObjectItemCaseSensitive(p,"ageMs");
    if (!cJSON_IsNumber(v) || v->valuedouble!=1 || !cJSON_IsString(session) || !cJSON_IsBool(visible) ||
        !cJSON_IsNumber(seq) || !isfinite(seq->valuedouble) || seq->valuedouble<0 || seq->valuedouble>9007199254740991.0 ||
        floor(seq->valuedouble)!=seq->valuedouble || !cJSON_IsNumber(age) || !isfinite(age->valuedouble) ||
        age->valuedouble<0 || age->valuedouble>500 || floor(age->valuedouble)!=age->valuedouble ||
        (cJSON_IsTrue(visible) && (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)))) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=tracking_observe(&controller,session->valuestring,(uint64_t)seq->valuedouble,cJSON_IsTrue(visible),
        cJSON_IsNumber(x)?(float)x->valuedouble:0,cJSON_IsNumber(y)?(float)y->valuedouble:0,(int)age->valuedouble,now_ms());
    xSemaphoreGive(lock); return ok;
}
cJSON *stand_tracking_status(void) {
    cJSON *p=cJSON_CreateObject(); xSemaphoreTake(lock,portMAX_DELAY);
    cJSON_AddBoolToObject(p,"outputEnabled",output);
    cJSON_AddItemToObject(p,"settings",settings_json(&controller.config,tracking_enabled));
    cJSON_AddBoolToObject(p,"anglesEstimated",true);
    cJSON_AddNumberToObject(p,"panDeg",(controller.pan_us-controller.config.pan.neutral_us)/US_PER_DEG);
    cJSON_AddNumberToObject(p,"tiltDeg",(controller.tilt_us-controller.config.tilt.neutral_us)/US_PER_DEG);
    cJSON_AddNumberToObject(p,"panUs",controller.pan_us);
    cJSON_AddNumberToObject(p,"tiltUs",controller.tilt_us);
    cJSON_AddStringToObject(p,"state",center_until_ms ?
        (commanded_pan_us==1500 && commanded_tilt_us==1500 ? "centering" : "manual") : tracking_state(&controller,now_ms()));
    cJSON_AddBoolToObject(p,"centerAvailable",stand_tracking_center_available());
    cJSON_AddBoolToObject(p,"manualControlAvailable",stand_tracking_center_available());
    cJSON_AddStringToObject(p,"manualAccess","owner_or_hub");
    cJSON_AddNumberToObject(p,"manualMinUs",SERVO_MANUAL_MIN_US);
    cJSON_AddNumberToObject(p,"manualMaxUs",SERVO_MANUAL_MAX_US);
    if (last_manual_ms) {
        cJSON_AddNumberToObject(p,"lastManualAtMs",last_manual_ms);
        cJSON_AddNumberToObject(p,"commandedPanUs",commanded_pan_us);
        cJSON_AddNumberToObject(p,"commandedTiltUs",commanded_tilt_us);
        cJSON_AddNumberToObject(p,"manualRemainingMs",center_until_ms>now_ms() ? center_until_ms-now_ms() : 0);
    }
    cJSON_AddNumberToObject(p,"panGpio",CONFIG_STAND_PAN_GPIO);
    cJSON_AddNumberToObject(p,"tiltGpio",CONFIG_STAND_TILT_GPIO);
    if (last_center_ms) {
        cJSON_AddNumberToObject(p,"lastCenterAtMs",last_center_ms);
        cJSON_AddNumberToObject(p,"centerPulseUs",1500);
        cJSON_AddNumberToObject(p,"centerRemainingMs",center_until_ms>now_ms() ? center_until_ms-now_ms() : 0);
    }
    if (controller.received) {
        cJSON_AddNumberToObject(p,"seq",(double)controller.seq);
        cJSON_AddNumberToObject(p,"ageMs",now_ms()-controller.received_ms+controller.age_ms);
        if (controller.visible) { cJSON_AddNumberToObject(p,"x",controller.x); cJSON_AddNumberToObject(p,"y",controller.y); }
    }
    xSemaphoreGive(lock); return p;
}
