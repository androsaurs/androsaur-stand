#include "stand_ota.h"
#include "stand_log.h"
#include "stand_security.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "mbedtls/sha256.h"
#include "sdkconfig.h"

#ifndef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "stand OTA requires CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y"
#endif

#define PREFIX_SIZE (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
#define UPLOAD_LIMIT_US (180LL * 1000000)
#define VERIFY_LIMIT_MS 120000
#define CONFIRM_MIN_US (15LL * 1000000)

static atomic_bool busy;
static atomic_bool pending;
static TaskHandle_t guard;
static SemaphoreHandle_t validation_lock;

static bool unhex(const char *s, unsigned char out[32]) {
    if (strlen(s) != 64) return false;
    for (int i=0; i<32; i++) {
        unsigned value=0;
        for (int j=0; j<2; j++) {
            char c=s[2*i+j];
            int digit=c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : -1;
            if (digit<0) return false;
            value=value*16+(unsigned)digit;
        }
        out[i]=(unsigned char)value;
    }
    return true;
}

static void guard_task(void *arg) {
    (void)arg;
    uint32_t command;
    for (;;) {
        if (xTaskNotifyWait(0, UINT32_MAX, &command, pdMS_TO_TICKS(1000))==pdTRUE) {
            // Give HTTPS enough time to deliver the accepted response.
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_restart();
        }
        xSemaphoreTake(validation_lock,portMAX_DELAY);
        if (pending && esp_timer_get_time()/1000 >= VERIFY_LIMIT_MS) {
            stand_log_event(STAND_LOG_OTA_ROLLBACK, 0, -127, "unconfirmed");
            // Even if rollback fails, reset: the bootloader will reject a pending image.
            esp_ota_mark_app_invalid_rollback_and_reboot();
            esp_restart();
        }
        xSemaphoreGive(validation_lock);
    }
}

void stand_ota_init(void) {
    validation_lock=xSemaphoreCreateMutex(); assert(validation_lock);
    esp_ota_img_states_t state;
    pending=esp_ota_get_state_partition(esp_ota_get_running_partition(), &state)==ESP_OK &&
            state==ESP_OTA_IMG_PENDING_VERIFY;
    // NVS logging and boot-slot validation run here too, not just a timer wait.
    assert(xTaskCreate(guard_task, "stand_ota_guard", 6144, NULL, 5, &guard)==pdPASS);
}

cJSON *stand_ota_status(void) {
    const esp_partition_t *running=esp_ota_get_running_partition();
    const esp_partition_t *next=esp_ota_get_next_update_partition(NULL);
    // esp_app_get_elf_sha256() follows CONFIG_APP_RETRIEVE_LEN_ELF_SHA and may be truncated.
    char sha[65];
    const unsigned char *elf=esp_app_get_description()->app_elf_sha256;
    const char *digits="0123456789abcdef";
    for (int i=0; i<32; i++) { sha[2*i]=digits[elf[i]>>4]; sha[2*i+1]=digits[elf[i]&15]; }
    sha[64]=0;
    cJSON *p=cJSON_CreateObject();
    cJSON_AddStringToObject(p, "partition", running->label);
    cJSON_AddStringToObject(p, "state", pending ? "pending_verify" : "valid");
    cJSON_AddStringToObject(p, "elfSha256", sha);
    cJSON_AddStringToObject(p, "version", esp_app_get_description()->version);
    cJSON_AddBoolToObject(p, "busy", busy);
    cJSON_AddBoolToObject(p, "rollbackEnabled", true);
    cJSON_AddNumberToObject(p, "confirmWithinS", VERIFY_LIMIT_MS/1000);
    cJSON_AddNumberToObject(p, "maxImageBytes", next ? next->size : 0);
    return p;
}

static bool header(httpd_req_t *req, const char *name, char *out, size_t n) {
    return httpd_req_get_hdr_value_str(req, name, out, n)==ESP_OK;
}

static esp_err_t reject(httpd_req_t *req, httpd_err_code_t code, const char *message) {
    // Never keep unread upload bytes on a reusable connection.
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send_err(req, code, message);
    // Returning an error closes this session synchronously in the HTTP task.
    return ESP_FAIL;
}

esp_err_t stand_ota_upload(httpd_req_t *req) {
    char content_type[40], device[40], hash[65]; unsigned char expected[32];
    if (!header(req,"Content-Type",content_type,sizeof(content_type)) || strcmp(content_type,"application/octet-stream") ||
        !header(req,"X-Stand-Id",device,sizeof(device)) || strcmp(device,stand_id) ||
        !header(req,"X-Image-SHA256",hash,sizeof(hash)) || !unhex(hash,expected))
        return reject(req,HTTPD_400_BAD_REQUEST,"binary content type, stand ID and SHA256 required");
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if (!target || req->content_len<PREFIX_SIZE || req->content_len>target->size)
        return reject(req,HTTPD_400_BAD_REQUEST,"image length outside OTA slot bounds");
    bool idle=false;
    if (pending || !atomic_compare_exchange_strong(&busy,&idle,true))
        return reject(req,HTTPD_400_BAD_REQUEST,"update busy or running image not confirmed");

    unsigned char *buffer=malloc(4096);
    esp_ota_handle_t ota=0; bool opened=false;
    mbedtls_sha256_context digest; mbedtls_sha256_init(&digest);
    const char *error="out of memory";
    esp_err_t err=ESP_ERR_NO_MEM;
    int64_t deadline=esp_timer_get_time()+UPLOAD_LIMIT_US;
    size_t received=0;
    if (!buffer) goto failed;
    // Reject a wrong chip/project before erasing or writing flash.
    while (received<PREFIX_SIZE) {
        if (esp_timer_get_time()>deadline) { error="upload deadline exceeded"; goto failed; }
        int n=httpd_req_recv(req,(char *)buffer+received,PREFIX_SIZE-received);
        if (n<=0) { error="incomplete image header"; goto failed; }
        received+=(size_t)n;
    }
    esp_image_header_t image; esp_app_desc_t app;
    memcpy(&image,buffer,sizeof(image));
    memcpy(&app,buffer+sizeof(image)+sizeof(esp_image_segment_header_t),sizeof(app));
    if (image.magic!=ESP_IMAGE_HEADER_MAGIC || image.chip_id!=ESP_CHIP_ID_ESP32S3 ||
        app.magic_word!=ESP_APP_DESC_MAGIC_WORD ||
        memcmp(app.project_name,"androsaur-stand\0",sizeof("androsaur-stand")) ||
        !memchr(app.version,0,sizeof(app.version))) {
        error="not an ESP32-S3 androsaur-stand app image"; goto failed;
    }
    err=mbedtls_sha256_starts(&digest,0);
    if (err!=0) { error="SHA256 initialization failed"; goto failed; }
    err=esp_ota_begin(target,req->content_len,&ota);
    if (err!=ESP_OK) { error="cannot start OTA"; goto failed; }
    opened=true;
    stand_log_event(STAND_LOG_OTA_START,0,-127,target->label);
    size_t chunk=received;
    for (;;) {
        if (mbedtls_sha256_update(&digest,buffer,chunk)!=0) { error="SHA256 update failed"; goto failed; }
        err=esp_ota_write(ota,buffer,chunk);
        if (err!=ESP_OK) { error="flash write failed"; goto failed; }
        if (received==req->content_len) break;
        if (esp_timer_get_time()>deadline) { error="upload deadline exceeded"; goto failed; }
        size_t remaining=req->content_len-received;
        int n=httpd_req_recv(req,(char *)buffer,remaining>4096 ? 4096 : remaining);
        if (n<=0) { error="upload interrupted"; goto failed; }
        chunk=(size_t)n; received+=chunk;
    }
    unsigned char actual[32];
    if (mbedtls_sha256_finish(&digest,actual)!=0 || memcmp(expected,actual,32)) {
        error="SHA256 mismatch"; goto failed;
    }
    err=esp_ota_end(ota); opened=false;
    if (err!=ESP_OK) { error="ESP image verification failed"; goto failed; }
    // This is the only point that changes the next boot slot.
    err=esp_ota_set_boot_partition(target);
    if (err!=ESP_OK) { error="cannot select boot slot"; goto failed; }
    stand_log_event(STAND_LOG_OTA_STAGED,0,-127,target->label);
    mbedtls_sha256_free(&digest); free(buffer);
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Connection","close");
    err=httpd_resp_sendstr(req,"{\"ok\":true,\"rebooting\":true,\"confirmationRequired\":true}");
    xTaskNotify(guard,1,eSetValueWithOverwrite);
    return err;
failed:
    if (opened) esp_ota_abort(ota);
    mbedtls_sha256_free(&digest); free(buffer);
    stand_log_event(STAND_LOG_OTA_FAILED,(int)err,-127,error);
    busy=false;
    return reject(req,HTTPD_400_BAD_REQUEST,error);
}

esp_err_t stand_ota_confirm(httpd_req_t *req) {
    char hash[65]; unsigned char requested[32];
    if (req->content_len!=0 || !header(req,"X-ELF-SHA256",hash,sizeof(hash)) || !unhex(hash,requested) ||
        memcmp(requested,esp_app_get_description()->app_elf_sha256,32))
        return reject(req,HTTPD_400_BAD_REQUEST,"running ELF SHA256 required");
    xSemaphoreTake(validation_lock,portMAX_DELAY);
    if (pending) {
        if (esp_timer_get_time()<CONFIRM_MIN_US) {
            xSemaphoreGive(validation_lock);
            return reject(req,HTTPD_400_BAD_REQUEST,"wait for 15 seconds of stable startup");
        }
        esp_err_t err=esp_ota_mark_app_valid_cancel_rollback();
        if (err!=ESP_OK) {
            xSemaphoreGive(validation_lock);
            return reject(req,HTTPD_500_INTERNAL_SERVER_ERROR,"confirmation failed");
        }
        pending=false;
        stand_log_event(STAND_LOG_OTA_CONFIRMED,0,-127,esp_ota_get_running_partition()->label);
    }
    xSemaphoreGive(validation_lock);
    httpd_resp_set_type(req,"application/json");
    return httpd_resp_sendstr(req,"{\"ok\":true,\"state\":\"valid\"}");
}
