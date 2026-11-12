#include "stand_security.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "esp_mac.h"
#include "esp_random.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha256.h"

char stand_id[32], stand_pin[65], stand_cert[1600], stand_private_key[400];
static nvs_handle_t store;
static SemaphoreHandle_t lock;
static char owner[65], owner_hash[65], hub_hash[65];
static int random_bytes(void *ctx, unsigned char *out, size_t n) {
    (void)ctx; esp_fill_random(out, n); return 0;
}
static void hex(const unsigned char *in, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) sprintf(out + i * 2, "%02x", in[i]);
}
void stand_random_hex(char *out, size_t n) {
    unsigned char bytes[32]; assert(n <= sizeof(bytes));
    esp_fill_random(bytes, n); hex(bytes, n, out);
}
void stand_setup_secret(char out[33]) {
    size_t size=33;
    esp_err_t err=nvs_get_str(store,"setup_pop",out,&size);
    if (err==ESP_ERR_NVS_NOT_FOUND) {
        stand_random_hex(out,16);
        ESP_ERROR_CHECK(nvs_set_str(store,"setup_pop",out)); ESP_ERROR_CHECK(nvs_commit(store));
    } else ESP_ERROR_CHECK(err);
    assert(strlen(out)==32 && strspn(out,"0123456789abcdef")==32);
}
static void digest(const char *key, char out[65]) {
    unsigned char bytes[32];
    ESP_ERROR_CHECK(mbedtls_sha256((const unsigned char *)key, strlen(key), bytes, 0));
    hex(bytes, sizeof(bytes), out);
}
static void load(const char *key, char *out, size_t n) {
    if (nvs_get_str(store, key, out, &n) != ESP_OK) out[0] = 0;
}
static bool valid_key(const char *key) {
    return key && strlen(key) == 64 && strspn(key, "0123456789abcdef") == 64;
}
bool stand_valid_owner(const char *phone, const char *key) {
    return phone && phone[0] && strlen(phone)<=64 &&
        strspn(phone,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")==strlen(phone) && valid_key(key);
}
static bool equal(const char *a, const char *b) {
    unsigned diff = 0;
    for (int i = 0; i < 64; i++) diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
    return diff == 0;
}
void stand_security_init(void) {
    lock=xSemaphoreCreateRecursiveMutex(); assert(lock);
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(stand_id, sizeof(stand_id), "stand-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_ERROR_CHECK(nvs_open("stand", NVS_READWRITE, &store));
    char binding[130]={0}; size_t binding_size=sizeof(binding);
    esp_err_t binding_err=nvs_get_blob(store,"binding",binding,&binding_size);
    assert(binding_err==ESP_ERR_NVS_NOT_FOUND || (binding_err==ESP_OK && binding_size==sizeof(binding)));
    if (binding_err==ESP_OK) { memcpy(owner,binding,65); memcpy(owner_hash,binding+65,65); }
    load("hub_hash", hub_hash, sizeof(hub_hash));
    load("cert", stand_cert, sizeof(stand_cert)); load("key", stand_private_key, sizeof(stand_private_key));
    if (!stand_cert[0] || !stand_private_key[0]) {
        mbedtls_pk_context key; mbedtls_pk_init(&key);
        ESP_ERROR_CHECK(mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)));
        ESP_ERROR_CHECK(mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), random_bytes, NULL));
        mbedtls_x509write_cert cert; mbedtls_x509write_crt_init(&cert);
        mbedtls_x509write_crt_set_version(&cert, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_x509write_crt_set_md_alg(&cert, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_subject_key(&cert, &key); mbedtls_x509write_crt_set_issuer_key(&cert, &key);
        char name[80]; snprintf(name, sizeof(name), "CN=%s.local", stand_id);
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_subject_name(&cert, name));
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_issuer_name(&cert, name));
        unsigned char serial[16]; esp_fill_random(serial, sizeof(serial)); serial[0] &= 0x7f; serial[0] |= 1;
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_serial_raw(&cert, serial, sizeof(serial)));
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_validity(&cert, "20260101000000", "20560101000000"));
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_basic_constraints(&cert, 0, -1));
        ESP_ERROR_CHECK(mbedtls_x509write_crt_set_key_usage(&cert, MBEDTLS_X509_KU_DIGITAL_SIGNATURE));
        ESP_ERROR_CHECK(mbedtls_x509write_crt_pem(&cert, (unsigned char *)stand_cert, sizeof(stand_cert), random_bytes, NULL));
        ESP_ERROR_CHECK(mbedtls_pk_write_key_pem(&key, (unsigned char *)stand_private_key, sizeof(stand_private_key)));
        ESP_ERROR_CHECK(nvs_set_str(store, "cert", stand_cert)); ESP_ERROR_CHECK(nvs_set_str(store, "key", stand_private_key));
        ESP_ERROR_CHECK(nvs_commit(store));
        mbedtls_x509write_crt_free(&cert); mbedtls_pk_free(&key);
    }
    mbedtls_x509_crt cert; mbedtls_x509_crt_init(&cert);
    ESP_ERROR_CHECK(mbedtls_x509_crt_parse(&cert, (unsigned char *)stand_cert, strlen(stand_cert) + 1));
    unsigned char bytes[32]; ESP_ERROR_CHECK(mbedtls_sha256(cert.raw.p, cert.raw.len, bytes, 0));
    hex(bytes, sizeof(bytes), stand_pin); mbedtls_x509_crt_free(&cert);
}
bool stand_owned(void) { xSemaphoreTakeRecursive(lock,portMAX_DELAY); bool owned=owner[0] && owner_hash[0]; xSemaphoreGiveRecursive(lock); return owned; }
void stand_owner_id(char out[65]) { xSemaphoreTakeRecursive(lock,portMAX_DELAY); strcpy(out,owner); xSemaphoreGiveRecursive(lock); }
static esp_err_t claim_locked(const char *phone, const char *key) {
    if (!stand_valid_owner(phone,key)) return ESP_ERR_INVALID_ARG;
    char hash[65]; digest(key, hash);
    if (stand_owned() && (strcmp(owner, phone) || !equal(owner_hash, hash))) return ESP_ERR_INVALID_STATE;
    char binding[130]={0}; strcpy(binding,phone); strcpy(binding+65,hash);
    ESP_ERROR_CHECK(nvs_set_blob(store,"binding",binding,sizeof(binding)));
    ESP_ERROR_CHECK(nvs_commit(store)); strcpy(owner, phone); strcpy(owner_hash, hash); return ESP_OK;
}
esp_err_t stand_claim(const char *phone, const char *key) {
    xSemaphoreTakeRecursive(lock,portMAX_DELAY); esp_err_t result=claim_locked(phone,key); xSemaphoreGiveRecursive(lock); return result;
}
static bool authorized_locked(const char *key, bool owner_only) {
    if (!valid_key(key)) return false;
    char hash[65]; digest(key, hash);
    return (owner_hash[0] && equal(hash, owner_hash)) || (!owner_only && hub_hash[0] && equal(hash, hub_hash));
}
bool stand_authorized(const char *key, bool owner_only) {
    xSemaphoreTakeRecursive(lock,portMAX_DELAY); bool result=authorized_locked(key,owner_only); xSemaphoreGiveRecursive(lock); return result;
}
static esp_err_t delegate_locked(const char *key) {
    if (!key || (key[0] && !valid_key(key))) return ESP_ERR_INVALID_ARG;
    char hash[65] = ""; if (key[0]) digest(key, hash);
    if (key[0] && owner_hash[0] && equal(hash,owner_hash)) return ESP_ERR_INVALID_ARG;
    ESP_ERROR_CHECK(nvs_set_str(store, "hub_hash", hash)); ESP_ERROR_CHECK(nvs_commit(store));
    strcpy(hub_hash, hash); return ESP_OK;
}
void stand_forget_owner(void) {
    xSemaphoreTakeRecursive(lock,portMAX_DELAY);
    const char *keys[] = {"binding", "hub_hash"};
    for (int i=0; i<2; i++) { esp_err_t err=nvs_erase_key(store, keys[i]); assert(err==ESP_OK || err==ESP_ERR_NVS_NOT_FOUND); }
    ESP_ERROR_CHECK(nvs_commit(store)); owner[0] = owner_hash[0] = hub_hash[0] = 0;
    xSemaphoreGiveRecursive(lock);
}
esp_err_t stand_delegate(const char *key) {
    xSemaphoreTakeRecursive(lock,portMAX_DELAY); esp_err_t result=delegate_locked(key); xSemaphoreGiveRecursive(lock); return result;
}
