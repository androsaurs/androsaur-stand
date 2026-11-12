#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
extern char stand_id[32], stand_pin[65];
extern char stand_cert[1600], stand_private_key[400];
void stand_security_init(void);
bool stand_owned(void);
void stand_owner_id(char out[65]);
esp_err_t stand_claim(const char *phone, const char *key);
bool stand_valid_owner(const char *phone, const char *key);
bool stand_authorized(const char *key, bool owner_only);
esp_err_t stand_delegate(const char *key);
void stand_forget_owner(void);
void stand_random_hex(char *out, size_t bytes);
void stand_setup_secret(char out[33]);
