#pragma once
#if defined ESP32


#include <tcpip_adapter.h>
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "mdns.h"

#include "EspToolkit.h"
#include "tasks/Telnet/Telnet.h"

#define TAG "network"

/**
 * @brief   Network
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/

#define DEFAULT_SCAN_LIST_SIZE 32
class Network{

    private:

        EspToolkit* tk;
        int16_t  calcRSSI(int32_t);

    public:

        //Global
        Network(EspToolkit*);
        Telnet*  telnet;
        void     commit();
        static   esp_err_t wifi_event_handler(void *ctx, system_event_t *event);
        void     getApIpStr(char* buf);
        void     getStaIpStr(char* buf);

        //LOW LEVEL CONFIG
        wifi_init_config_t config_init = {
            .event_handler = &esp_event_send,
            .osi_funcs = &g_wifi_osi_funcs,
            .wpa_crypto_funcs = g_wifi_default_wpa_crypto_funcs,
            .static_rx_buf_num = CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM,
            .dynamic_rx_buf_num = CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM,
            .tx_buf_type = CONFIG_ESP32_WIFI_TX_BUFFER_TYPE,
            .static_tx_buf_num = WIFI_STATIC_TX_BUFFER_NUM,
            .dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUFFER_NUM,
            .csi_enable = WIFI_CSI_ENABLED,
            .ampdu_rx_enable = WIFI_AMPDU_RX_ENABLED,
            .ampdu_tx_enable = WIFI_AMPDU_TX_ENABLED,
            .nvs_enable = WIFI_NVS_ENABLED,
            .nano_enable = WIFI_NANO_FORMAT_ENABLED,
            .tx_ba_win = WIFI_DEFAULT_TX_BA_WIN,
            .rx_ba_win = WIFI_DEFAULT_RX_BA_WIN,
            .wifi_task_core_id = WIFI_TASK_CORE_ID,
            .beacon_max_len = WIFI_SOFTAP_BEACON_MAX_LEN, 
            .mgmt_sbuf_num = WIFI_MGMT_SBUF_NUM, 
            .feature_caps = g_wifi_feature_caps, 
            .magic = WIFI_INIT_CONFIG_MAGIC
        };

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
            }
        };

        //CONFIG
        bool        ap_enable{false};
        std::string ap_password{""};
        bool        sta_enable{false};
        std::string sta_network{""};
        std::string sta_password{""};
        std::string sta_ip{"0.0.0.0"};
        std::string sta_subnet{"0.0.0.0"};
        std::string sta_gateway{"0.0.0.0"};
        std::string sta_dns{"0.0.0.0"};
        bool        telnet_enable{true};
        int         ps_type{1};         // WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM
    
};

#endif