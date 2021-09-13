#pragma once

#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_eth.h"
#include "esp_sntp.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "mdns.h"
#include "driver/spi_master.h"

#include "EspToolkit.h"
#include "tasks/Telnet/Telnet.h"

/**
 * @brief   Network
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/

#define NETWORK_DEFAULT_SCAN_LIST_SIZE 32
class Network{

    private:

        static Network* _this;
        bool begin{false};

    public:

        //Global
        Network();
        Telnet*  telnet{nullptr};
        void     commit();
        static void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
        esp_netif_t* netif_hotspot{nullptr};
        esp_netif_t* netif_wifi{nullptr};
        esp_netif_t* netif_eth{nullptr};
        esp_eth_handle_t netif_eth_handle{nullptr};
        int16_t  calcRSSI(int32_t);
        bool     hotspot_autostart_triggered{false};

        //CONFIG
        wifi_config_t config_ap = {
            .ap = {
                .ssid = {},
                .password = {},
                .ssid_len = 0,
                .channel = 0,
                .authmode = WIFI_AUTH_OPEN,
                .ssid_hidden = 0,
                .max_connection = 4,
                .beacon_interval = 100,
            }
        };

          wifi_config_t config_sta = {
            .sta = {
                .ssid = {},
                .password = {},
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .bssid_set = false,
                .bssid     = {},
                .channel = 0,
                .listen_interval = 3,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold = {
                    .rssi = -100,
                    .authmode = WIFI_AUTH_OPEN
                },
                .pmf_cfg = {
                    .capable = true,
                    .required = false
                }
            }
        };

        bool        ap_enable{false};
        bool        ap_autostart{false};
        bool        wifi_enable{false};
        std::string wifi_network;
        std::string wifi_password;
        bool        eth_enable{false};
        std::string ip;
        std::string subnet;
        std::string gateway;
        std::string dns;
        std::string sntp;
        bool        telnet_enable{true};
        int         wifi_powerSave{1};     
        std::string ota_caCert;
    
};