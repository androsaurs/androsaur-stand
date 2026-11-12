idf_component_register(SRCS "main.c" "stand_security.c" "stand_http.c" INCLUDE_DIRS "."
    REQUIRES esp_wifi protocomm nvs_flash esp_https_server mbedtls json esp_driver_gpio esp_driver_uart vfs esp_timer esp_hw_support bootloader_support)
