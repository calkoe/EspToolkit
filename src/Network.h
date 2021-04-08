#pragma once
#if defined ESP32

#include <WiFi.h> 
#include <HTTPUpdate.h>
#include <Update.h>
#include "EspToolkit.h"
#include "esp_wifi.h"
#include <tcpip_adapter.h>

#define EVT_NET_PREFIX "net:"

/**
 * @brief   Network
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Network{

    private:

        EspToolkit* tk;
        int16_t  calcRSSI(int32_t);

    public:

        //Global
        Network(EspToolkit*);
        void    commit();
        static esp_err_t wifi_event_handler(void *ctx, system_event_t *event);
        char*   getApIpStr(char* buf);
        char*   getStaIpStr(char* buf);

        //CONFIG
        bool        sta_enable{false};
        std::string sta_network{""};
        std::string sta_password{""};
        std::string sta_ip{"0.0.0.0"};
        std::string sta_subnet{"0.0.0.0"};
        std::string sta_gateway{"0.0.0.0"};
        std::string sta_dns{"0.0.0.0"};
        bool        ap_enable{false};
        std::string ap_password{""};
    
};

#endif