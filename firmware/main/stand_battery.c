#include "stand_battery.h"
#include "battery_core.h"
#include "charger_manual_core.h"
#include "sdkconfig.h"
#include "stand_security.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <assert.h>
#include <math.h>
#include <string.h>
_Static_assert(CONFIG_STAND_CHARGE_START_PERCENT < CONFIG_STAND_CHARGE_STOP_PERCENT, "Charging start must be below stop");
static battery_controller controller;
static SemaphoreHandle_t lock;
static bool output;
static bool effective_allow;
static charger_override override;
#ifdef CONFIG_STAND_CHARGER_OUTPUT
static const bool automatic_enabled=true;
#else
static const bool automatic_enabled=false;
#endif
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static int level(bool allow) {
#ifdef CONFIG_STAND_CHARGER_ACTIVE_LOW
    return !allow;
#else
    return allow;
#endif
}
static void apply(void) {
    effective_allow=output && charger_override_apply(&override,automatic_enabled,controller.allow,now_ms());
    if (output) ESP_ERROR_CHECK(gpio_set_level(CONFIG_STAND_CHARGER_GPIO,level(effective_allow)));
}
static void loop(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000)); xSemaphoreTake(lock,portMAX_DELAY);
        battery_tick(&controller,now_ms());
        // Ownership removal must not leave an unattended phone without power.
        if (!stand_owned()) controller.allow=true;
        apply(); xSemaphoreGive(lock);
    }
}
void stand_battery_init(void) {
    battery_init(&controller,CONFIG_STAND_CHARGE_START_PERCENT,CONFIG_STAND_CHARGE_STOP_PERCENT);
    lock=xSemaphoreCreateMutex(); assert(lock);
#if defined(CONFIG_STAND_CHARGER_OUTPUT) || defined(CONFIG_STAND_CHARGER_MANUAL_CONTROL)
    int pin=CONFIG_STAND_CHARGER_GPIO;
    // Restrict to available pins on this N16R8; exclude boot/USB and configured servos.
    if (pin>=1 && pin<=21 && pin!=3 && pin!=19 && pin!=20 &&
        pin!=CONFIG_STAND_PAN_GPIO && pin!=CONFIG_STAND_TILT_GPIO && controller.low<controller.high) {
        ESP_ERROR_CHECK(gpio_set_level(pin,level(automatic_enabled)));
        gpio_config_t cfg={.pin_bit_mask=1ULL<<pin,.mode=GPIO_MODE_INPUT_OUTPUT};
        ESP_ERROR_CHECK(gpio_config(&cfg)); output=true;
    }
#endif
    xTaskCreate(loop,"stand_battery",3072,NULL,5,NULL);
}
bool stand_charger_manual(const cJSON *p) {
#ifndef CONFIG_STAND_CHARGER_MANUAL_CONTROL
    return false;
#else
    if (!cJSON_IsObject(p)) return false;
    const cJSON *mode=cJSON_GetObjectItemCaseSensitive(p,"mode"),
        *enabled=cJSON_GetObjectItemCaseSensitive(p,"enabled"),
        *duration=cJSON_GetObjectItemCaseSensitive(p,"durationMs");
    bool automatic=cJSON_IsString(mode) && !strcmp(mode->valuestring,"auto");
    if (automatic) {
        if (enabled || duration) return false;
    } else if (mode || !cJSON_IsBool(enabled) || !cJSON_IsNumber(duration) ||
        !isfinite(duration->valuedouble) || duration->valuedouble<1000 ||
        duration->valuedouble>CHARGER_MANUAL_MAX_MS || floor(duration->valuedouble)!=duration->valuedouble) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=output;
    if (ok) {
        if (automatic) override.active=false;
        else ok=charger_override_set(&override,cJSON_IsTrue(enabled),duration->valueint,now_ms());
        apply();
    }
    xSemaphoreGive(lock); return ok;
#endif
}
bool stand_battery_observe(const cJSON *p) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(p,"v"), *session=cJSON_GetObjectItemCaseSensitive(p,"session"),
        *seq=cJSON_GetObjectItemCaseSensitive(p,"seq"), *percent=cJSON_GetObjectItemCaseSensitive(p,"percent"),
        *charging=cJSON_GetObjectItemCaseSensitive(p,"charging"), *age=cJSON_GetObjectItemCaseSensitive(p,"ageMs");
    if (!cJSON_IsNumber(v) || v->valuedouble!=1 || !cJSON_IsString(session) || !cJSON_IsBool(charging) ||
        !cJSON_IsNumber(seq) || !isfinite(seq->valuedouble) || seq->valuedouble<0 || seq->valuedouble>9007199254740991.0 || floor(seq->valuedouble)!=seq->valuedouble ||
        !cJSON_IsNumber(percent) || !isfinite(percent->valuedouble) || percent->valuedouble<0 || percent->valuedouble>100 || floor(percent->valuedouble)!=percent->valuedouble ||
        !cJSON_IsNumber(age) || !isfinite(age->valuedouble) || age->valuedouble<0 || age->valuedouble>60000 || floor(age->valuedouble)!=age->valuedouble) return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool ok=battery_observe(&controller,session->valuestring,(uint64_t)seq->valuedouble,(int)percent->valuedouble,cJSON_IsTrue(charging),(int)age->valuedouble,now_ms());
    if (ok) apply();
    xSemaphoreGive(lock); return ok;
}
cJSON *stand_battery_status(void) {
    cJSON *p=cJSON_CreateObject(); xSemaphoreTake(lock,portMAX_DELAY);
    battery_tick(&controller,now_ms()); apply();
    cJSON_AddBoolToObject(p,"outputEnabled",output); cJSON_AddBoolToObject(p,"chargeAllowed",effective_allow);
    cJSON_AddBoolToObject(p,"policyChargeAllowed",controller.allow);
    cJSON_AddBoolToObject(p,"automaticEnabled",automatic_enabled);
    cJSON_AddBoolToObject(p,"manualActive",override.active);
    cJSON_AddStringToObject(p,"controlMode",override.active ? (override.enabled ? "manual_on" : "manual_off") : automatic_enabled ? "auto" : "off");
    cJSON_AddNumberToObject(p,"manualRemainingMs",override.active && override.until_ms>now_ms() ? override.until_ms-now_ms() : 0);
    cJSON_AddNumberToObject(p,"gpio",CONFIG_STAND_CHARGER_GPIO);
    if (output) cJSON_AddNumberToObject(p,"gpioLevel",gpio_get_level(CONFIG_STAND_CHARGER_GPIO));
    cJSON_AddNumberToObject(p,"startPercent",controller.low); cJSON_AddNumberToObject(p,"stopPercent",controller.high);
    bool fresh=controller.received && now_ms()-controller.received_ms+controller.age_ms<=600000;
    cJSON_AddStringToObject(p,"state",!controller.received?"waiting":!fresh?"stale":controller.allow?"allowed":"paused");
    if (controller.received) {
        cJSON_AddNumberToObject(p,"percent",controller.percent); cJSON_AddBoolToObject(p,"charging",controller.charging);
        cJSON_AddNumberToObject(p,"ageMs",now_ms()-controller.received_ms+controller.age_ms);
        cJSON_AddNumberToObject(p,"seq",(double)controller.seq);
    }
    xSemaphoreGive(lock); return p;
}
