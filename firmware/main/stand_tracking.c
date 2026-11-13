#include "stand_tracking.h"
#include "tracking_core.h"
#include <assert.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static tracking_controller controller;
static SemaphoreHandle_t lock;
static bool output;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static bool gpio_valid(int pin) { return pin >= 1 && pin <= 21 && pin != 3 && pin != 19 && pin != 20; }
static uint32_t duty(float us) { return (uint32_t)lroundf(us * 16384 / 20000); }
static void loop(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
        xSemaphoreTake(lock, portMAX_DELAY); tracking_tick(&controller, now_ms());
        float pan = controller.pan_us, tilt = controller.tilt_us; xSemaphoreGive(lock);
        if (output) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty(pan)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty(tilt)); ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        }
    }
}
void stand_tracking_init(void) {
    tracking_config cfg = {
        .pan={CONFIG_STAND_PAN_MIN_US,CONFIG_STAND_PAN_MAX_US,CONFIG_STAND_PAN_NEUTRAL_US,1},
        .tilt={CONFIG_STAND_TILT_MIN_US,CONFIG_STAND_TILT_MAX_US,CONFIG_STAND_TILT_NEUTRAL_US,1},
        .gain_us=40,.max_step_us=20,.slew_us_per_s=100,.deadband=.04f
    };
#ifdef CONFIG_STAND_PAN_REVERSE
    cfg.pan.direction=-1;
#endif
#ifdef CONFIG_STAND_TILT_REVERSE
    cfg.tilt.direction=-1;
#endif
    tracking_init(&controller, &cfg, now_ms()); lock=xSemaphoreCreateMutex(); assert(lock);
#ifdef CONFIG_STAND_SERVO_OUTPUT
    const bool requested=true;
#else
    const bool requested=false;
#endif
    if (requested && tracking_config_valid(&cfg) && gpio_valid(CONFIG_STAND_PAN_GPIO) && gpio_valid(CONFIG_STAND_TILT_GPIO) &&
        CONFIG_STAND_PAN_GPIO != CONFIG_STAND_TILT_GPIO) {
        ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_14_BIT,
            .timer_num=LEDC_TIMER_0,.freq_hz=50,.clk_cfg=LEDC_AUTO_CLK};
        ESP_ERROR_CHECK(ledc_timer_config(&timer));
        ledc_channel_config_t pan={.gpio_num=CONFIG_STAND_PAN_GPIO,.speed_mode=LEDC_LOW_SPEED_MODE,
            .channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=duty(cfg.pan.neutral_us)};
        ledc_channel_config_t tilt=pan; tilt.gpio_num=CONFIG_STAND_TILT_GPIO; tilt.channel=LEDC_CHANNEL_1; tilt.duty=duty(cfg.tilt.neutral_us);
        ESP_ERROR_CHECK(ledc_channel_config(&pan)); ESP_ERROR_CHECK(ledc_channel_config(&tilt)); output=true;
    } else if (requested) ESP_LOGW("tracking", "Servo output disabled: unconfirmed GPIO or calibration");
    xTaskCreate(loop,"stand_tracking",3072,NULL,5,NULL);
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
    cJSON_AddStringToObject(p,"state",tracking_state(&controller,now_ms()));
    if (controller.received) {
        cJSON_AddNumberToObject(p,"seq",(double)controller.seq);
        cJSON_AddNumberToObject(p,"ageMs",now_ms()-controller.received_ms+controller.age_ms);
        if (controller.visible) { cJSON_AddNumberToObject(p,"x",controller.x); cJSON_AddNumberToObject(p,"y",controller.y); }
    }
    xSemaphoreGive(lock); return p;
}
