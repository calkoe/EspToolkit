#pragma once
#include "Arduino.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define CONFIG_WIFI_SSID "WLAN"
#define CONFIG_WIFI_AP "MYAP"
#define CONFIG_WIFI_HOSTNAME "ESP32TK"
#define CONFIG_WIFI_PASSWORD "JHD6-NO2H-7H0K-1SFF-37DF"

namespace Network{

    static const char *TAG = "wifi station";

    static esp_err_t event_handler(void *ctx, system_event_t *event);
    void init();

};