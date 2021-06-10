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

#define DEFAULT_SCAN_LIST_SIZE 32
class Network{

    private:

        static Network* _this;
        EspToolkit* tk{nullptr};
        bool begin{false};

    public:

        //Global
        Network(EspToolkit*);
        Telnet*  telnet{nullptr};
        void     commit();
        static void network_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
        esp_netif_t* netif_ap{nullptr};
        esp_netif_t* netif_sta{nullptr};
        esp_netif_t* netif_eth{nullptr};
        esp_eth_handle_t netif_eth_handle{nullptr};
        int16_t  calcRSSI(int32_t);
        bool     ap_autostart_triggered{false};

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

        //CONFIG
        bool        ap_enable{false};
        bool        ap_autostart{true};
        bool        sta_enable{false};
        std::string sta_network;
        std::string sta_password;
        bool        eth_enable{false};
        std::string static_ip;
        std::string static_subnet;
        std::string static_gateway;
        std::string static_dns;
        std::string static_sntp;
        bool        telnet_enable{true};
        int         ps_type{1};         // WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM
        std::string ota_caCert;
    
};