/* Label-based BLE setup. No computer or internal button in the user's workflow. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bootloader_random.h"
#include "mdns.h"
#include "protocomm.h"
#include "protocomm_ble.h"
#include "protocomm_security2.h"
#include "esp_srp.h"
#include "cJSON.h"
#include "stand_security.h"
#include "stand_http.h"

static const char *TAG="stand";
static bool mdns_started;
static atomic_bool restarting;
static atomic_bool configured;
static int64_t recovery_until;
static char setup_code[33], service_name[20];
static char *setup_salt, *setup_verifier;
static protocomm_security2_params_t setup_security;
static const uint8_t service_uuid[16]={0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0xff,0xff,0x00,0x00};
static protocomm_ble_name_uuid_t endpoints[]={
    {"proto-ver",0xff51},{"prov-session",0xff52},{"stand-owner",0xff53}
};

static void reconnect(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_wifi_disconnect(); esp_wifi_connect();
    vTaskDelete(NULL);
}
static void restart_later(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
}
static esp_err_t exchange(uint32_t session, const uint8_t *input, ssize_t len,
                          uint8_t **output, ssize_t *outlen, void *ctx) {
    (void)session; (void)ctx;
    cJSON *request=len>0 && len<=512 ? cJSON_ParseWithLength((const char *)input,len) : NULL;
    cJSON *phone=cJSON_GetObjectItemCaseSensitive(request,"phoneId");
    cJSON *key=cJSON_GetObjectItemCaseSensitive(request,"key");
    cJSON *op=cJSON_GetObjectItemCaseSensitive(request,"op");
    cJSON *reply=cJSON_CreateObject();
    bool ok=false;
    const char *error="invalid_request";
    if (cJSON_IsString(op) && strcmp(op->valuestring,"info")==0) {
        ok=true;
        char owner[65]; stand_owner_id(owner);
        cJSON_AddStringToObject(reply,"phoneId",owner);
        cJSON_AddBoolToObject(reply,"recovery",esp_timer_get_time()<recovery_until);
        // Authenticated BLE is also a discovery fallback when LAN multicast is filtered.
        esp_netif_ip_info_t network={0};
        esp_netif_t *station=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        wifi_ap_record_t ap;
        if (station && esp_wifi_sta_get_ap_info(&ap)==ESP_OK &&
            esp_netif_get_ip_info(station,&network)==ESP_OK && network.ip.addr) {
            char address[16]; snprintf(address,sizeof(address),IPSTR,IP2STR(&network.ip));
            cJSON_AddStringToObject(reply,"ip",address);
        }
    } else if (cJSON_IsString(op) && strcmp(op->valuestring,"networks")==0) {
        ok=esp_wifi_scan_start(NULL,true)==ESP_OK;
        if (ok) {
            uint16_t count=12;
            wifi_ap_record_t *records=calloc(count,sizeof(wifi_ap_record_t));
            cJSON *networks=cJSON_AddArrayToObject(reply,"networks");
            if (records && esp_wifi_scan_get_ap_records(&count,records)==ESP_OK) {
                for (int i=0;i<count && cJSON_GetArraySize(networks)<4;i++) {
                    const char *ssid=(const char *)records[i].ssid;
                    bool duplicate=false;
                    cJSON *entry;
                    cJSON_ArrayForEach(entry,networks) { if (!strcmp(entry->valuestring,ssid)) duplicate=true; }
                    if (ssid[0] && !duplicate) cJSON_AddItemToArray(networks,cJSON_CreateString(ssid));
                }
            }
            free(records);
        } else error="wifi_scan_unavailable";
    } else if (!restarting && cJSON_IsString(op) && strcmp(op->valuestring,"release")==0) {
        if (cJSON_IsString(key) && stand_authorized(key->valuestring,true)) {
            stand_forget_owner(); configured=false;
            ESP_ERROR_CHECK(esp_wifi_restore());
            restarting=true; ok=true;
            xTaskCreate(restart_later,"stand_restart",2048,NULL,5,NULL);
        } else error="owner_required";
    } else if (!restarting && cJSON_IsString(phone) && cJSON_IsString(key)) {
        cJSON *ssid=cJSON_GetObjectItemCaseSensitive(request,"ssid");
        cJSON *password=cJSON_GetObjectItemCaseSensitive(request,"password");
        bool valid=stand_valid_owner(phone->valuestring,key->valuestring) && cJSON_IsString(ssid) && strlen(ssid->valuestring)>0 && strlen(ssid->valuestring)<=32 &&
            cJSON_IsString(password) && strlen(password->valuestring)<=63;
        if (valid) {
            // Recovery requires both the physical label secret and an explicit power-cycle window.
            cJSON *recover=cJSON_GetObjectItemCaseSensitive(request,"recover");
            if (cJSON_IsTrue(recover) && esp_timer_get_time()<recovery_until) {
                stand_forget_owner(); recovery_until=0;
            }
            if (stand_claim(phone->valuestring,key->valuestring)==ESP_OK) {
                wifi_config_t wifi={0};
                memcpy(wifi.sta.ssid,ssid->valuestring,strlen(ssid->valuestring));
                memcpy(wifi.sta.password,password->valuestring,strlen(password->valuestring));
                ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
                ok=esp_wifi_set_config(WIFI_IF_STA,&wifi)==ESP_OK;
                if (ok) { configured=true; xTaskCreate(reconnect,"stand_wifi",3072,NULL,5,NULL); }
                else error="wifi_configuration_failed";
            } else error="owned_by_another_phone";
        }
    }
    cJSON_Delete(request);
    cJSON_AddBoolToObject(reply,"ok",ok);
    if (ok) { cJSON_AddStringToObject(reply,"id",stand_id); cJSON_AddStringToObject(reply,"fingerprint",stand_pin); }
    else cJSON_AddStringToObject(reply,"error",error);
    *output=(uint8_t *)cJSON_PrintUnformatted(reply); cJSON_Delete(reply);
    if (!*output) return ESP_ERR_NO_MEM;
    *outlen=strlen((char *)*output); return ESP_OK;
}

static void start_ble(void) {
    protocomm_t *pc=protocomm_new(); assert(pc);
    protocomm_ble_config_t config={.nu_lookup_count=3,.nu_lookup=endpoints};
    strcpy(config.device_name,service_name); memcpy(config.service_uuid,service_uuid,16);
    ESP_ERROR_CHECK(protocomm_ble_start(pc,&config));
    ESP_ERROR_CHECK(protocomm_set_version(pc,"proto-ver",
        "{\"prov\":{\"ver\":\"androsaur-2\",\"sec_ver\":2,\"sec_patch_ver\":1,\"cap\":[\"androsaur-label-v1\"]}}"));
    ESP_ERROR_CHECK(protocomm_set_security(pc,"prov-session",&protocomm_security2,&setup_security));
    ESP_ERROR_CHECK(protocomm_add_endpoint(pc,"stand-owner",exchange,NULL));
    ESP_LOGI(TAG,"BLE setup ready: %s (scan the label)",service_name);
}
static void events(void *ctx, esp_event_base_t base, int32_t id, void *data) {
    (void)ctx; (void)data;
    if (base==WIFI_EVENT && configured && !restarting &&
        (id==WIFI_EVENT_STA_START || id==WIFI_EVENT_STA_DISCONNECTED)) esp_wifi_connect();
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP && stand_owned()) {
        stand_http_start();
        if (!mdns_started) {
            ESP_ERROR_CHECK(mdns_init()); ESP_ERROR_CHECK(mdns_hostname_set(stand_id));
            mdns_txt_item_t txt[]={{"id",stand_id},{"protocol","1"}};
            ESP_ERROR_CHECK(mdns_service_add(stand_id,"_androsaur-stand","_tcp",8443,txt,2)); mdns_started=true;
        }
        ESP_LOGI(TAG,"network ready: %s, HTTPS port 8443",stand_id);
    }
}
static void console(void *arg) {
    (void)arg;
    char line[40];
    for (;;) {
        if (!fgets(line,sizeof(line),stdin)) { clearerr(stdin); vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (strcmp(line,"factory-label\n") && strcmp(line,"factory-label\r\n")) continue;
        // Manufacturing-only export: never emitted during normal startup or over the network.
        cJSON *label=cJSON_CreateObject();
        cJSON_AddStringToObject(label,"kind","androsaur-stand"); cJSON_AddNumberToObject(label,"v",1);
        cJSON_AddStringToObject(label,"id",stand_id); cJSON_AddStringToObject(label,"name",service_name);
        cJSON_AddStringToObject(label,"pop",setup_code); cJSON_AddStringToObject(label,"fingerprint",stand_pin);
        char *json=cJSON_PrintUnformatted(label); cJSON_Delete(label);
        if (json) { printf("STAND_LABEL:%s\n",json); fflush(stdout); free(json); }
    }
}
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    bootloader_random_enable(); stand_security_init(); stand_setup_secret(setup_code);
    int verifier_len=0;
    ESP_ERROR_CHECK(esp_srp_gen_salt_verifier("androsaur",9,setup_code,32,&setup_salt,16,&setup_verifier,&verifier_len));
    setup_security=(protocomm_security2_params_t){.salt=setup_salt,.salt_len=16,.verifier=setup_verifier,.verifier_len=verifier_len};
    bootloader_random_disable();
    snprintf(service_name,sizeof(service_name),"ANDRO_%s",stand_id+strlen(stand_id)-6);
    nvs_handle_t settings; ESP_ERROR_CHECK(nvs_open("stand_boot",NVS_READWRITE,&settings));
    uint8_t boots=0; nvs_get_u8(settings,"fast",&boots);
    if (stand_owned() && esp_reset_reason()==ESP_RST_POWERON) boots++;
    else boots=0;
    if (boots>=3) { recovery_until=esp_timer_get_time()+120LL*1000000; boots=0; }
    ESP_ERROR_CHECK(nvs_set_u8(settings,"fast",boots)); ESP_ERROR_CHECK(nvs_commit(settings));
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default()); esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&wifi));
    wifi_config_t saved={0}; ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA,&saved)); configured=saved.sta.ssid[0]!=0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,events,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,events,NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_start());
    // Mains-powered peripheral: keep LAN discovery/ARP and control responsive while BLE advertises.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    start_ble();
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0,1024,0,0,NULL,0)); esp_vfs_dev_uart_use_driver(UART_NUM_0);
    xTaskCreate(console,"stand_factory",4096,NULL,2,NULL);
    gpio_config_t button={.pin_bit_mask=1ULL<<GPIO_NUM_0,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&button)); int held=0; bool stable=false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (!stable && esp_timer_get_time()>30LL*1000000) { ESP_ERROR_CHECK(nvs_set_u8(settings,"fast",0)); ESP_ERROR_CHECK(nvs_commit(settings)); stable=true; }
        if (gpio_get_level(GPIO_NUM_0)==0) held++;
        else if (held) {
            // Legacy physical emergency reset; normal setup never requires opening the case.
            if (held>=100) { stand_forget_owner(); ESP_ERROR_CHECK(esp_wifi_restore()); esp_restart(); }
            held=0;
        }
    }
}
