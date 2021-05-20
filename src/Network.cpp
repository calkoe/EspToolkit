#include <Network.h>

Network* Network::_this{nullptr};

Network::Network(EspToolkit* tk):tk{tk}{
    if(_this) return;
    _this = this;

    // NETIF
    esp_netif_init();
    mdns_init();
    esp_event_loop_create_default();
    esp_event_handler_instance_t instance_wifi_any_id;
    esp_event_handler_instance_t instance_ip_any_id;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,  &wifi_event_handler, this,   &instance_wifi_any_id);
    esp_event_handler_instance_register(IP_EVENT,   ESP_EVENT_ANY_ID,  &wifi_event_handler, this,   &instance_ip_any_id);
    netif_ap  = esp_netif_create_default_wifi_ap();
    netif_sta = esp_netif_create_default_wifi_sta();

    // TELNET
    if(telnet_enable){
        telnet = new Telnet(&(this->tk->events),(char*)EVT_TK_COMMAND,(char*)EVT_TK_BROADCAST);
    }

    // AP BUTTON TOGGLE
    tk->button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,1000,(char*)"bootbutton1000ms");
    tk->events.on(0,"bootbutton1000ms",[](void* ctx, void* arg){
        Network*    _this = (Network*) ctx;
        if(!*(bool*)arg){
            ESP_LOGI("NETWORK", "TOGGLE AP");
            _this->ap_enable = !_this->ap_enable;
            _this->tk->variableLoad(true);
            _this->commit();
        }
    },this);

    // AP Autostart
    if(_this->ap_autostart){
        tk->timer.setInterval([](void* ctx){
            Network*    _this = (Network*) ctx;
            if(!_this->ap_enable && !_this->ap_autostart_triggered && (!_this->sta_enable || !_this->tk->status[STATUS_BIT_NETWORK])){
                _this->ap_autostart_triggered = true;
                _this->ap_enable = true;
                _this->commit();
            }else if(_this->ap_enable && _this->ap_autostart_triggered && _this->sta_enable && _this->tk->status[STATUS_BIT_NETWORK]){
                _this->ap_autostart_triggered = false;
                _this->ap_enable = false;
                _this->commit();
            }
        },this,10000,"hotspot autostart");
    }

    // CONFIG VARIABLES
    tk->variableAdd("wifi/powerSafe",    ps_type,           "📶 WiFI Power Safe: 0=WIFI_PS_NONE | 1=WIFI_PS_MIN_MODEM | 2=WIFI_PS_MAX_MODEM");
    tk->variableAdd("wifi/enable",       sta_enable,        "📶 Enable WiFi STA");
    tk->variableAdd("wifi/network",      sta_network,       "📶 Network SSID");
    tk->variableAdd("wifi/password",     sta_password,      "📶 Network Password");
    tk->variableAdd("wifi/ip",           sta_ip,            "📶 IP      (leave blank to use DHCP)");
    tk->variableAdd("wifi/subnet",       sta_subnet,        "📶 Subnet  (leave blank to use DHCP)");
    tk->variableAdd("wifi/gateway",      sta_gateway,       "📶 Gateway (leave blank to use DHCP)");
    tk->variableAdd("wifi/dns",          sta_dns,           "📶 DNS     (leave blank to use DHCP)");
    tk->variableAdd("wifi/sntp",         sta_sntp,          "📶 SNTP Server for time synchronisation (leave blank to disable)");
    tk->variableAdd("hotspot/enable",    ap_enable,         "📶 Enable WiFi Hotspot");
    tk->variableAdd("hotspot/autostart", ap_autostart,      "📶 Start Hotspot if threre is no STA connection aviable");
    tk->variableAdd("telnet/enable",     telnet_enable,     "📶 Enables Telnet Server on Port 23");
    tk->variableAdd("ota/caCert",        ota_caCert,        "📶 CA Certificate for OTA Updates");

    // COMMANDS
    tk->commandAdd("wifiStatus",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        Network*    _this   = (Network*) ctx;
        EspToolkit* tk      = (EspToolkit*) _this->tk;
        char OUT[LONG];
        reply((char*)"📶 Newtwork:\r\n");
        wifi_mode_t mode;
        const char* m;
        if(esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT){
            m = "ESP_ERR_WIFI_NOT_INIT";
        }else{
            switch(mode){
                case WIFI_MODE_NULL:     m = "WIFI_MODE_NULL";break;
                case WIFI_MODE_STA:      m = "WIFI_MODE_STA";break;
                case WIFI_MODE_AP:       m = "WIFI_MODE_AP";break;
                case WIFI_MODE_APSTA:    m = "WIFI_MODE_APSTA";break;
                default:                 m = "UNKOWN";
            }
        };
        snprintf(OUT,LONG,"%-30s : %s\r\n","Mode",m);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Hostname",tk->hostname.c_str());reply(OUT);
        esp_netif_ip_info_t ipInfoAp, ipInfoSta;
        esp_netif_get_ip_info(_this->netif_ap, &ipInfoAp);
        esp_netif_get_ip_info(_this->netif_sta, &ipInfoSta);
        uint8_t macAp[6], macSta[6];
        esp_wifi_get_mac(WIFI_IF_AP, macAp);
        esp_wifi_get_mac(WIFI_IF_STA, macSta);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP IP",IP2STR(&ipInfoAp.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP MASK",IP2STR(&ipInfoAp.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP GW",IP2STR(&ipInfoAp.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "AP MAC", macAp[0], macAp[1], macAp[2], macAp[3], macAp[4], macAp[5]);reply(OUT);
        wifi_sta_list_t clients;
        esp_wifi_ap_get_sta_list(&clients); 
        snprintf(OUT,LONG,"%-30s : %d\r\n","AP Stations",clients.num);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","STA Connected",tk->status[STATUS_BIT_NETWORK]?"true":"false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA IP",IP2STR(&ipInfoSta.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA MASK",IP2STR(&ipInfoSta.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA GW",IP2STR(&ipInfoSta.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA DNS",IP2STR(&dns_getserver(0)->u_addr.ip4));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "STA MAC", macSta[0], macSta[1], macSta[2], macSta[3], macSta[4], macSta[5]);reply(OUT);
        wifi_ap_record_t infoSta;
        esp_wifi_sta_get_ap_info(&infoSta);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n","STA BSSID",infoSta.bssid[0], infoSta.bssid[1], infoSta.bssid[2], infoSta.bssid[3], infoSta.bssid[4], infoSta.bssid[5]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d dBm (%d%%)\r\n","RSSI",infoSta.rssi,_this->calcRSSI(infoSta.rssi));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","Primary channel",infoSta.primary);reply(OUT);
        const char* wifi_second_chan;
        if(infoSta.second == WIFI_SECOND_CHAN_NONE)  wifi_second_chan = "WIFI_SECOND_CHAN_NONE";
        if(infoSta.second == WIFI_SECOND_CHAN_ABOVE) wifi_second_chan = "WIFI_SECOND_CHAN_ABOVE";
        if(infoSta.second == WIFI_SECOND_CHAN_BELOW) wifi_second_chan = "WIFI_SECOND_CHAN_BELOW";
        snprintf(OUT,LONG,"%-30s : %s\r\n","Secondary channel",wifi_second_chan);reply(OUT);
        if(_this->telnet) snprintf(OUT,LONG,"%-30s : %s\r\n","Telnet connected",_this->telnet->clientSock > 0 ? "true" : "false");
    },this,  "📶 Wifi status");
    
    tk->commandAdd("wifiOta",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){

        if(parCnt==2){

            // Config OTA
            esp_http_client_config_t config = {};
            config.url = param[1];
            config.transport_type = HTTP_TRANSPORT_OVER_TCP;
            config.cert_pem = _this->ota_caCert.c_str();
            config.skip_cert_common_name_check = true;

            // Run OTA
            esp_err_t ret = esp_https_ota(&config);
            if (ret == ESP_OK) {
                esp_restart();
            } else {
                reply(strerror(ret));reply(" - Firmware upgrade failed\r\n");
            }

        }else{
            _this->tk->commandMan("wifiOta",reply);
        }
    },this,"📶 [url] | load and install new firmware from URL (https only!)");

    tk->commandAdd("wifiScan",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if(mode == WIFI_MODE_NULL){esp_wifi_set_mode(WIFI_MODE_STA);}
        uint16_t number = DEFAULT_SCAN_LIST_SIZE;
        wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
        uint16_t ap_count = 0;
        memset(ap_info, 0, sizeof(ap_info));
        wifi_scan_config_t config{};
        reply((char*)"Scaning Networks...\r\n");
        esp_err_t scanResult = esp_wifi_scan_start(&config, true);
        if(scanResult == ESP_OK) {
            esp_wifi_scan_get_ap_records(&number, ap_info);
            esp_wifi_scan_get_ap_num(&ap_count);
            for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
                const char* authmode{""};
                const char* pairwise_cipher{""};
                const char* group_cipher{""};
                switch (ap_info[i].authmode) {
                    case WIFI_AUTH_OPEN:            authmode = (char*)"WIFI_AUTH_OPEN";break;
                    case WIFI_AUTH_WEP:             authmode = (char*)"WIFI_AUTH_WEP";break;
                    case WIFI_AUTH_WPA_PSK:         authmode = (char*)"WIFI_AUTH_WPA_PSK";break;
                    case WIFI_AUTH_WPA2_PSK:        authmode = (char*)"WIFI_AUTH_WPA2_PSK";break;
                    case WIFI_AUTH_WPA_WPA2_PSK:    authmode = (char*)"WIFI_AUTH_WPA_WPA2_PSK";break;
                    case WIFI_AUTH_WPA2_ENTERPRISE: authmode = (char*)"WIFI_AUTH_WPA2_ENTERPRISE";break;
                    //case WIFI_AUTH_WPA3_PSK: authmode = "WIFI_AUTH_WPA3_PSK";break;
                    //case WIFI_AUTH_WPA2_WPA3_PSK: authmode = "WIFI_AUTH_WPA2_WPA3_PSK";break;
                    default: authmode = "WIFI_AUTH_UNKNOWN";break;
                }
                if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                    switch (ap_info[i].pairwise_cipher){
                        case WIFI_CIPHER_TYPE_NONE:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_NONE";break;
                        case WIFI_CIPHER_TYPE_WEP40:        pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_WEP40";break;
                        case WIFI_CIPHER_TYPE_WEP104:       pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_WEP104";break;
                        case WIFI_CIPHER_TYPE_TKIP:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_TKIP";break;
                        case WIFI_CIPHER_TYPE_CCMP:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_CCMP";break;
                        case WIFI_CIPHER_TYPE_TKIP_CCMP:    pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_TKIP_CCMP";break;
                        default:  pairwise_cipher = "WIFI_CIPHER_TYPE_UNKNOWN";break;
                    }
                    switch (ap_info[i].group_cipher) {
                        case WIFI_CIPHER_TYPE_NONE:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_NONE";break;
                        case WIFI_CIPHER_TYPE_WEP40:        pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_WEP40";break;
                        case WIFI_CIPHER_TYPE_WEP104:       pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_WEP104";break;
                        case WIFI_CIPHER_TYPE_TKIP:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_TKIP";break;
                        case WIFI_CIPHER_TYPE_CCMP:         pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_CCMP";break;
                        case WIFI_CIPHER_TYPE_TKIP_CCMP:    pairwise_cipher = (char*)"WIFI_CIPHER_TYPE_TKIP_CCMP";break;
                        default:  pairwise_cipher = "WIFI_CIPHER_TYPE_UNKNOWN";break;
                    }
                }
                snprintf(OUT,LONG,"%-30s : %-3d dBm (%-3d%%) | Authmode: %s | Pairwise_cipher: %s | Group_cipher: %s\r\n",ap_info[i].ssid, ap_info[i].rssi, _this->calcRSSI(ap_info[i].rssi),authmode,pairwise_cipher,group_cipher);reply(OUT);
            }
        }else{
            snprintf(OUT,LONG,"Scaning failed: %s\r\n",esp_err_to_name(scanResult));
            reply(OUT);
        }
    },this, "📶 Scans for nearby networks");

    tk->commandAdd("wifiCommit",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
        EspToolkit* tk    = (EspToolkit*) _this->tk;
        if(parCnt>=2){
                snprintf(OUT,LONG,"Set  wifi/enabled: %s\r\n","true");reply(OUT);
                _this->sta_enable  = true;
                snprintf(OUT,LONG,"Set  wifi/network: %s\r\n",param[1]);reply(OUT);
                _this->sta_network = param[1];
        };
        if(parCnt>=3){
                snprintf(OUT,LONG,"Set wifi/password: %s\r\n",param[2]);reply(OUT);
                _this->sta_password = param[2];
        };
        tk->variableLoad(true);
        reply((char*)"DONE! ✅ > Type 'wifiStatus' to check status\r\n");
        _this->commit();

    },this, "📶 [network] [password] | apply network settings and connect to configured network",false);
    
    tk->commandAdd("wifiDns",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
        EspToolkit* tk    = (EspToolkit*) _this->tk;
        if(parCnt>=2){
            ip_addr_t ip_Addr;
            dns_gethostbyname(param[1], &ip_Addr, [](const char *name, const ip_addr_t* ipaddr, void *callback_arg){
                if(ipaddr) *(ip_addr_t*)callback_arg = *ipaddr;
            }, &ip_Addr);
            for(uint8_t i{0};i<10;i++){
                if((uint32_t)ip_Addr.u_addr.ip4.addr){
                    snprintf(OUT,LONG,"✅ DNS Resolved %s -> %d.%d.%d.%d\r\n",param[1],IP2STR(&ip_Addr.u_addr.ip4));reply(OUT);
                    return;
                }
                vTaskDelay(100);
            }
            snprintf(OUT,LONG,"❌ DNS lookup timeout\r\n");reply(OUT);
        }else{
            tk->commandMan("wifiDns",reply);
        }
    },this,  "📶 [ip] | resolve DNS");

};

void Network::commit(){

    tk->status[STATUS_BIT_NETWORK] = true;

    // Config
    bool sta = sta_enable && !sta_network.empty();
    bool ap  = ap_enable  && !tk->hostname.empty();
    tk->hostname.copy((char*)config_ap.ap.ssid,32,0);
    tk->password.copy((char*)config_ap.ap.password,64,0);
    sta_network.copy((char*)config_sta.sta.ssid,32,0);
    sta_password.copy((char*)config_sta.sta.password,64,0);

    wifi_init_config_t config_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config_init);
    esp_wifi_set_ps((wifi_ps_type_t)ps_type);

    if(sta && ap)   esp_wifi_set_mode(WIFI_MODE_APSTA);
    else if(ap)     esp_wifi_set_mode(WIFI_MODE_AP);
    else if(sta)    esp_wifi_set_mode(WIFI_MODE_STA);
    else            esp_wifi_set_mode(WIFI_MODE_NULL);

    if(ap){
        esp_wifi_set_config(WIFI_IF_AP, &config_ap);
    }

    if(sta){
        esp_wifi_set_config(WIFI_IF_STA, &config_sta);
        if(!sta_ip.empty() && !sta_subnet.empty() && !sta_gateway.empty() && !sta_dns.empty()){
            esp_netif_dhcps_stop(netif_sta);
            esp_netif_ip_info_t ip_info;
            ip_info.ip.addr       = static_cast<uint32_t>(ipaddr_addr(sta_ip.c_str()));
            ip_info.gw.addr       = static_cast<uint32_t>(ipaddr_addr(sta_gateway.c_str()));
            ip_info.netmask.addr  = static_cast<uint32_t>(ipaddr_addr(sta_subnet.c_str()));
            esp_netif_set_ip_info(netif_sta,&ip_info);
            esp_netif_dns_info_t dns;
	        dns.ip.type = ESP_IPADDR_TYPE_V4;
	        dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(ipaddr_addr(sta_dns.c_str()));
            esp_netif_set_dns_info(netif_sta,ESP_NETIF_DNS_MAIN,&dns);
        }
    }

    // MDNS
    mdns_hostname_set(_this->tk->hostname.c_str());
    mdns_instance_name_set(_this->tk->hostname.c_str());
    mdns_service_add("Telnet Server ESP32", "_telnet", "_tcp", 23, NULL, 0);

    // SNTP
    if(!sta_sntp.empty()){
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, (char*)sta_sntp.c_str());
        sntp_set_sync_interval(10 * 60 * 1000);
        sntp_init();
    }

    esp_wifi_start();
    ESP_LOGI("NETWORK", "COMMIT FINISHED");

};

void Network::wifi_event_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data)
{ 
    Network* _this = (Network*)ctx;
    switch (event_id) {
        case WIFI_EVENT_AP_START:
            ESP_LOGI("NETWORK", "WIFI_EVENT_AP_START");
            _this->tk->events.emit("WIFI_EVENT_AP_START");
            esp_netif_set_hostname(_this->netif_ap,_this->tk->hostname.c_str());
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI("NETWORK", "SYSTEM_EVENT_AP_STACONNECTED");
            _this->tk->events.emit("SYSTEM_EVENT_AP_STACONNECTED");
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STACONNECTED");
            _this->tk->events.emit("WIFI_EVENT_AP_STACONNECTED");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STADISCONNECTED");
            _this->tk->events.emit("WIFI_EVENT_AP_STADISCONNECTED");
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STOP");
            _this->tk->events.emit("WIFI_EVENT_AP_STOP");
            break;

        case WIFI_EVENT_STA_START:
            ESP_LOGI("NETWORK", "WIFI_EVENT_STA_START");
            _this->tk->events.emit("WIFI_EVENT_STA_START");
            _this->tk->status[STATUS_BIT_NETWORK] = false;
            esp_netif_set_hostname(_this->netif_sta,_this->tk->hostname.c_str());
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI("NETWORK", "WIFI_EVENT_STA_CONNECTED");
            _this->tk->events.emit("WIFI_EVENT_STA_CONNECTED");
            _this->tk->status[STATUS_BIT_NETWORK] = true;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI("NETWORK", "WIFI_EVENT_STA_DISCONNECTED");
            _this->tk->events.emit("WIFI_EVENT_STA_DISCONNECTED");
            _this->tk->status[STATUS_BIT_NETWORK] = false;
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI("NETWORK", "WIFI_EVENT_STA_STOP");
            _this->tk->events.emit("WIFI_EVENT_STA_STOP");
            _this->tk->status[STATUS_BIT_NETWORK] = false;
            break;

        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI("NETWORK", "IP_EVENT_STA_GOT_IP");
            _this->tk->events.emit("IP_EVENT_STA_GOT_IP");
            _this->tk->status[STATUS_BIT_NETWORK] = true;
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI("NETWORK", "IP_EVENT_STA_LOST_IP");
            _this->tk->events.emit("IP_EVENT_STA_LOST_IP");
            _this->tk->status[STATUS_BIT_NETWORK] = false;
            break;

        default:
            break;
    }
}

int16_t Network::calcRSSI(int32_t r){
    return min(max(2 * (r + 100.0), 0.0), 100.0);
};