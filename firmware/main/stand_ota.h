#pragma once
#include "esp_http_server.h"
#include "cJSON.h"

void stand_ota_init(void);
cJSON *stand_ota_status(void);
// These handlers must only be registered behind owner authentication.
esp_err_t stand_ota_upload(httpd_req_t *req);
esp_err_t stand_ota_confirm(httpd_req_t *req);
