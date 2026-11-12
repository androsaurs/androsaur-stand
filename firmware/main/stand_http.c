#include "stand_http.h"
#include "stand_security.h"
#include <string.h>
#include <stdlib.h>
#include "esp_https_server.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
static httpd_handle_t server;
static bool quiet;
static bool auth(httpd_req_t *req, bool owner) {
    char header[80];
    if (httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) != ESP_OK ||
        strncmp(header, "Bearer ", 7) || !stand_authorized(header + 7, owner)) {
        httpd_resp_set_status(req, "401 Unauthorized"); httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
        httpd_resp_sendstr(req, "{\"error\":\"unauthorized\"}"); return false;
    }
    return true;
}
static esp_err_t json(httpd_req_t *req, cJSON *obj) {
    char *text = cJSON_PrintUnformatted(obj); cJSON_Delete(obj);
    if (!text) return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json"); httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text); free(text); return err;
}
static cJSON *body(httpd_req_t *req) {
    if (req->content_len < 2 || req->content_len > 256) return NULL;
    char buf[257]; size_t n = 0;
    while (n < req->content_len) { int got = httpd_req_recv(req, buf+n, req->content_len-n); if (got<=0) return NULL; n+=got; }
    buf[n]=0; return cJSON_ParseWithLength(buf,n);
}
static esp_err_t status(httpd_req_t *req) {
    if (!auth(req, false)) return ESP_OK;
    cJSON *obj=cJSON_CreateObject(); cJSON_AddStringToObject(obj,"id",stand_id);
    char phone[65]; stand_owner_id(phone); cJSON_AddStringToObject(obj,"phoneId",phone);
    cJSON_AddStringToObject(obj,"firmware","0.2.2-ble-discovery"); cJSON_AddNumberToObject(obj,"protocol",1);
    cJSON_AddNumberToObject(obj,"uptimeS",esp_timer_get_time()/1000000); cJSON_AddBoolToObject(obj,"quiet",quiet);
    cJSON *caps=cJSON_AddArrayToObject(obj,"capabilities");
    cJSON_AddItemToArray(caps,cJSON_CreateString("status")); cJSON_AddItemToArray(caps,cJSON_CreateString("policy"));
    return json(req,obj);
}
static esp_err_t hub(httpd_req_t *req) {
    if (!auth(req,true)) return ESP_OK;
    cJSON *obj=body(req), *key=cJSON_GetObjectItemCaseSensitive(obj,"key");
    bool ok=cJSON_IsString(key) && stand_delegate(key->valuestring)==ESP_OK; cJSON_Delete(obj);
    if (!ok) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"bad key");
    return httpd_resp_sendstr(req,"{\"ok\":true}");
}
static esp_err_t policy(httpd_req_t *req) {
    if (!auth(req,false)) return ESP_OK;
    cJSON *obj=body(req), *value=cJSON_GetObjectItemCaseSensitive(obj,"quiet");
    if (!cJSON_IsBool(value)) { cJSON_Delete(obj); return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"quiet must be boolean"); }
    quiet=cJSON_IsTrue(value); cJSON_Delete(obj);
    nvs_handle_t store; ESP_ERROR_CHECK(nvs_open("stand_policy",NVS_READWRITE,&store));
    ESP_ERROR_CHECK(nvs_set_u8(store,"quiet",quiet)); ESP_ERROR_CHECK(nvs_commit(store)); nvs_close(store);
    return status(req);
}
void stand_http_start(void) {
    if (server) return;
    nvs_handle_t store; uint8_t value=0;
    if (nvs_open("stand_policy",NVS_READONLY,&store)==ESP_OK) { nvs_get_u8(store,"quiet",&value); nvs_close(store); }
    quiet=value!=0;
    httpd_ssl_config_t cfg=HTTPD_SSL_CONFIG_DEFAULT(); cfg.port_secure=8443;
    cfg.httpd.stack_size=8192; cfg.httpd.max_open_sockets=3; cfg.httpd.lru_purge_enable=true;
    cfg.servercert=(const unsigned char *)stand_cert; cfg.servercert_len=strlen(stand_cert)+1;
    cfg.prvtkey_pem=(const unsigned char *)stand_private_key; cfg.prvtkey_len=strlen(stand_private_key)+1;
    ESP_ERROR_CHECK(httpd_ssl_start(&server,&cfg));
    httpd_uri_t routes[]={ {.uri="/v1/status",.method=HTTP_GET,.handler=status},
        {.uri="/v1/hub",.method=HTTP_POST,.handler=hub}, {.uri="/v1/policy",.method=HTTP_POST,.handler=policy} };
    for (size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++) ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}
