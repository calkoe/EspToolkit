#include "Network.h"

static const char *TAG = "wifi station";

void Network::init(){

   
    wifi_config_t wifi_config_sta = {
        .sta = {
            {.ssid = CONFIG_WIFI_SSID},
            {.password = CONFIG_WIFI_PASSWORD},
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .bssid_set = false,
            .bssid     = {},
            .channel = 0,
            .listen_interval = 1,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold = {
                .rssi = -100,
                .authmode = WIFI_AUTH_OPEN
            },
        },
    };

     wifi_config_t wifi_config_ap = {
        .ap = {
            {.ssid = CONFIG_WIFI_AP},
            {.password = CONFIG_WIFI_PASSWORD},
            .ssid_len = 0,
            .channel = 0,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100,
        }
    };
    
    //Init
    esp_event_loop_init(event_handler, NULL);

    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    esp_wifi_set_mode(WIFI_MODE_APSTA);


    //STA
    //TCPIP STA STATIC IP START
    /*
    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);

    tcpip_adapter_ip_info_t ip_info;
    ip4addr_aton("192.168.1.200", &ip_info.ip);
    ip4addr_aton("255.255.255.0", &ip_info.gw);
    ip4addr_aton("192.168.1.1",   &ip_info.netmask);
    tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

    tcpip_adapter_dns_info_t dns_info;
    ipaddr_aton("192.168.1.1",&dns_info.ip);
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_BACKUP, &dns_info);
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_FALLBACK, &dns_info);
    */
    //TCPIP STA STATIC IP END
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta);
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,  CONFIG_WIFI_HOSTNAME);

    // AP
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap);
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, CONFIG_WIFI_HOSTNAME);
    tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    
    // Start
    esp_wifi_start();

};

static esp_err_t Network::event_handler(void *ctx, system_event_t *event)
{
    ESP_LOGI(TAG, (String)event->event_id);
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "got ip:%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            esp_wifi_connect();
            break;
        default:
            break;
        }
    return ESP_OK;
}