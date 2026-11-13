#include "stand_battery.h"
#include "battery_core.h"
#include "sdkconfig.h"
#include "stand_security.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <assert.h>
#include <math.h>
_Static_assert(CONFIG_STAND_CHARGE_START_PERCENT < CONFIG_STAND_CHARGE_STOP_PERCENT, "Charging start must be below stop");
static battery_controller controller;
static SemaphoreHandle_t lock;
static bool output;
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static int level(bool allow) {
#ifdef CONFIG_STAND_CHARGER_ACTIVE_LOW
    return !allow;
#else
    return allow;
#endif
}
static void apply(void) {
    if (output) ESP_ERROR_CHECK(gpio_set_level(CONFIG_STAND_CHARGER_GPIO,level(controller.allow)));
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
#ifdef CONFIG_STAND_CHARGER_OUTPUT
    int pin=CONFIG_STAND_CHARGER_GPIO;
    // Restrict to available pins on this N16R8; exclude boot/USB and configured servos.
    if (pin>=1 && pin<=21 && pin!=3 && pin!=19 && pin!=20 &&
        pin!=CONFIG_STAND_PAN_GPIO && pin!=CONFIG_STAND_TILT_GPIO && controller.low<controller.high) {
        ESP_ERROR_CHECK(gpio_set_level(pin,level(true)));
        gpio_config_t cfg={.pin_bit_mask=1ULL<<pin,.mode=GPIO_MODE_OUTPUT};
        ESP_ERROR_CHECK(gpio_config(&cfg)); output=true;
    }
#endif
    xTaskCreate(loop,"stand_battery",3072,NULL,5,NULL);
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
    cJSON_AddBoolToObject(p,"outputEnabled",output); cJSON_AddBoolToObject(p,"chargeAllowed",controller.allow);
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
