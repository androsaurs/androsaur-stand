#include "stand_log.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LOG_MAGIC 0x414C4F47u /* ALOG */
#define DETAIL_SIZE 24
#define LOG_KEY "events"

typedef struct {
    uint32_t sequence;
    uint32_t boot;
    uint32_t uptime_s;
    uint16_t event;
    int16_t reason;
    int16_t rssi;
    char detail[DETAIL_SIZE];
} log_record_t;

typedef struct {
    uint32_t magic;
    uint32_t next;
    uint32_t sequence;
    uint32_t boot;
    log_record_t records[STAND_LOG_CAPACITY];
} log_store_t;

static const char *TAG = "stand_log";
static nvs_handle_t store;
static SemaphoreHandle_t lock;
static log_store_t journal;

static const char *event_name(uint16_t event) {
    switch (event) {
        case STAND_LOG_BOOT: return "boot";
        case STAND_LOG_WIFI_START: return "wifi_start";
        case STAND_LOG_WIFI_DISCONNECTED: return "wifi_disconnected";
        case STAND_LOG_GOT_IP: return "got_ip";
        case STAND_LOG_HTTPS_READY: return "https_ready";
        default: return "unknown";
    }
}

static bool valid_journal(const log_store_t *value) {
    return value->magic == LOG_MAGIC && value->next < STAND_LOG_CAPACITY;
}

static void persist_locked(void) {
    esp_err_t err = nvs_set_blob(store, LOG_KEY, &journal, sizeof(journal));
    if (err == ESP_OK) err = nvs_commit(store);
    if (err != ESP_OK) ESP_LOGE(TAG, "journal commit failed: %s", esp_err_to_name(err));
}

void stand_log_init(void) {
    lock = xSemaphoreCreateMutex();
    assert(lock);
    ESP_ERROR_CHECK(nvs_open("stand_log", NVS_READWRITE, &store));
    size_t size = sizeof(journal);
    esp_err_t err = nvs_get_blob(store, LOG_KEY, &journal, &size);
    if (err != ESP_OK || size != sizeof(journal) || !valid_journal(&journal)) {
        memset(&journal, 0, sizeof(journal));
        journal.magic = LOG_MAGIC;
        journal.next = 0;
        journal.sequence = 0;
        journal.boot = 0;
    }
    journal.boot++;
    persist_locked();
}

void stand_log_event(stand_log_event_t event, int reason, int rssi, const char *detail) {
    if (!lock) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    log_record_t *record = &journal.records[journal.next];
    memset(record, 0, sizeof(*record));
    record->sequence = ++journal.sequence;
    record->boot = journal.boot;
    record->uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    record->event = (uint16_t)event;
    record->reason = (int16_t)reason;
    record->rssi = (int16_t)rssi;
    if (detail) strncpy(record->detail, detail, sizeof(record->detail) - 1);
    journal.next = (journal.next + 1) % STAND_LOG_CAPACITY;
    persist_locked();
    xSemaphoreGive(lock);
}

cJSON *stand_log_json(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON *events = cJSON_AddArrayToObject(root, "events");
    if (!events || !lock) return root;
    xSemaphoreTake(lock, portMAX_DELAY);
    cJSON_AddNumberToObject(root, "capacity", STAND_LOG_CAPACITY);
    cJSON_AddNumberToObject(root, "boot", journal.boot);
    uint32_t count = journal.sequence < STAND_LOG_CAPACITY ? journal.sequence : STAND_LOG_CAPACITY;
    uint32_t first = (journal.next + STAND_LOG_CAPACITY - count) % STAND_LOG_CAPACITY;
    for (uint32_t i = 0; i < count; i++) {
        const log_record_t *record = &journal.records[(first + i) % STAND_LOG_CAPACITY];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "seq", record->sequence);
        cJSON_AddNumberToObject(item, "boot", record->boot);
        cJSON_AddNumberToObject(item, "uptimeS", record->uptime_s);
        cJSON_AddStringToObject(item, "event", event_name(record->event));
        cJSON_AddNumberToObject(item, "reason", record->reason);
        cJSON_AddNumberToObject(item, "rssi", record->rssi);
        if (record->detail[0]) cJSON_AddStringToObject(item, "detail", record->detail);
        cJSON_AddItemToArray(events, item);
    }
    xSemaphoreGive(lock);
    return root;
}
