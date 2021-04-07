#pragma once
#if defined ESP32

#include <WiFi.h> 
#include <HTTPUpdate.h>
#include <Update.h>
#include "EspToolkit.h"
#include "esp_wifi.h"


#define EVT_NET_PREFIX "net:"

class Network{

    private:

        EspToolkit* tk;
        s16  calcRSSI(s32);

    public:

        //Global
        Network(EspToolkit*);
        void    commit();
        static esp_err_t wifi_event_handler(void *ctx, system_event_t *event);

        //CONFIG
        bool   sta_enable{false};
        String sta_network{""};
        String sta_password{""};
        String sta_ip{"0.0.0.0"};
        String sta_subnet{"0.0.0.0"};
        String sta_gateway{"0.0.0.0"};
        String sta_dns{"0.0.0.0"};
        bool   ap_enable{false};
        String ap_network{""};
        String ap_password{""};
    
};

#endif