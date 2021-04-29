#if defined ESP32

#include <Network.h>

Network* Network::_this{nullptr};

Network::Network(EspToolkit* tk):tk{tk}{
    if(_this) return;
    _this = this;

    // TCPIP
    tcpip_adapter_init();
    esp_event_loop_init(wifi_event_handler, this);

    // TELNET
    if(telnet_enable){
        telnet = new Telnet(&(this->tk->events),(char*)EVT_TK_COMMAND,(char*)EVT_TK_BROADCAST);
    }

    // BUTTON TOGGLE AP
    tk->button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,1000,(char*)"bootbutton1000ms");
    tk->events.on(0,"bootbutton1000ms",[](void* ctx, void* arg){
        Network*    _this = (Network*) ctx;
        if(!*(bool*)arg){
            ESP_LOGI(TAG, "TOGGLE AP");
            _this->ap_enable = !_this->ap_enable;
            _this->tk->variableLoad(true);
            _this->commit();
        }
    },this);

    // CONFIG VARIABLES
    tk->variableAdd("wifi/powerSafe",    ps_type,           "📶 WiFI Power Safe: 0=WIFI_PS_NONE | 1=WIFI_PS_MIN_MODEM | 2=WIFI_PS_MAX_MODEM");
    tk->variableAdd("wifi/enable",       sta_enable,        "📶 Enable WiFi STA-Mode");
    tk->variableAdd("wifi/network",      sta_network,       "📶 Network SSID");
    tk->variableAdd("wifi/password",     sta_password,      "📶 Network Password");
    tk->variableAdd("wifi/ip",           sta_ip,            "📶 IP (leave blank to use DHCP)");
    tk->variableAdd("wifi/subnet",       sta_subnet,        "📶 Subnet");
    tk->variableAdd("wifi/gateway",      sta_gateway,       "📶 Gateway");
    tk->variableAdd("wifi/dns",          sta_dns,           "📶 DNS");
    tk->variableAdd("hotspot/enable",    ap_enable,         "📶 Enable WiFi Hotspot-Mode");
    tk->variableAdd("telnet/enable",     telnet_enable,     "📶 Enables Telnet Server on Port 23");

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
        snprintf(OUT,LONG,"%-30s : %s\r\n","Connected",tk->status[STATUS_BIT_NETWORK]?"true":"false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Hostname",tk->hostname.c_str());reply(OUT);
        tcpip_adapter_ip_info_t ipInfoAp, ipInfoSta; 
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfoAp);
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfoSta);
        uint8_t macAp[6], macSta[6];
        esp_wifi_get_mac(WIFI_IF_AP, macAp);
        esp_wifi_get_mac(WIFI_IF_STA, macSta);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP IP",IP2STR(&ipInfoAp.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP MASK",IP2STR(&ipInfoAp.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP GW",IP2STR(&ipInfoAp.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "AP MAC", macAp[0], macAp[1], macAp[2], macAp[3], macAp[4], macAp[5]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA IP",IP2STR(&ipInfoSta.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA MASK",IP2STR(&ipInfoSta.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA GW",IP2STR(&ipInfoSta.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA DNS1",IP2STR(&dns_getserver(0)->u_addr.ip4));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA DNS2",IP2STR(&dns_getserver(1)->u_addr.ip4));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "STA MAC", macSta[0], macSta[1], macSta[2], macSta[3], macSta[4], macSta[5]);reply(OUT);
        wifi_ap_record_t infoSta;
        esp_wifi_sta_get_ap_info(&infoSta);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n","STA BSSID",infoSta.bssid[0], infoSta.bssid[1], infoSta.bssid[2], infoSta.bssid[3], infoSta.bssid[4], infoSta.bssid[5]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d dBm (%d%%)\r\n","RSSI",infoSta.rssi,_this->calcRSSI(infoSta.rssi));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","Primary channel",infoSta.primary);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","Secondary channel",infoSta.second);reply(OUT);
         wifi_sta_list_t clients;
        esp_wifi_ap_get_sta_list(&clients); 
        snprintf(OUT,LONG,"%-30s : %d\r\n","AP Stations",clients.num);reply(OUT);
        if(_this->telnet) snprintf(OUT,LONG,"%-30s : %s\r\n","Telnet connected",_this->telnet->clientSock > 0 ? "true" : "false");
    },this,  "📶 Wifi status");
    
    tk->commandAdd("wifiOta",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){

        if(parCnt==2){

            // Load cert_pem from SPIFFS 
            FILE* cert_pem_f = fopen("/spiffs/ca_cert.pem", "r");
            if (cert_pem_f == NULL) {
                ESP_LOGE(TAG, "Failed to open file (/spiffs/ca_cert.pem) for reading");
                return;
            }
            fseek(cert_pem_f, 0, SEEK_END);
            size_t cert_pem_size = ftell(cert_pem_f);
            char* cert_pem = new char[cert_pem_size+1];
            rewind(cert_pem_f);
            fread(cert_pem, sizeof(char), cert_pem_size, cert_pem_f);
            fclose(cert_pem_f);
            cert_pem[cert_pem_size] = '\0';

            // Config OTA
            esp_http_client_config_t config = {};
            config.url = param[1];
            config.transport_type = HTTP_TRANSPORT_OVER_TCP;
            config.cert_pem = cert_pem;
            config.skip_cert_common_name_check = true;

            // Run OTA
            esp_err_t ret = esp_https_ota(&config);
            if (ret == ESP_OK) {
                esp_restart();
            } else {
                reply(strerror(ret));reply(" - Firmware upgrade failed\r\n");
            }

            delete[] cert_pem;

        }else{
            _this->tk->commandMan("wifiOta",reply);
        }
    },this,"📶 [url] | load and install new firmware from URL (https only!)");

    tk->commandAdd("wifiScan",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
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
                tk->variableSet("wifi/network",param[1]);
        };
        if(parCnt>=3){
                snprintf(OUT,LONG,"Set wifi/password: %s\r\n",param[2]);reply(OUT);
                tk->variableSet("wifi/password",param[2]);
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
            ip_addr_t ip_Addr{0};
            dns_gethostbyname(param[1], &ip_Addr, [](const char *name, const ip_addr_t* ipaddr, void *callback_arg){
                if(ipaddr) *(ip_addr_t*)callback_arg = *ipaddr;
            }, &ip_Addr);
            for(uint8_t i{0};i<10;i++){
                if((u32_t)ip_Addr.u_addr.ip4.addr){
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

    //Config
    tk->status[STATUS_BIT_NETWORK] = true;
    bool sta = sta_enable && !sta_network.empty();
    bool ap  = ap_enable  && !tk->hostname.empty();
    tk->hostname.copy((char*)config_ap.ap.ssid,32,0);
    tk->password.copy((char*)config_ap.ap.password,64,0);
    sta_network.copy((char*)config_sta.sta.ssid,32,0);
    sta_password.copy((char*)config_sta.sta.password,64,0);

    esp_wifi_init(&config_init);
    esp_wifi_set_ps((wifi_ps_type_t)ps_type);

    if(sta && ap)   esp_wifi_set_mode(WIFI_MODE_APSTA);
    else if(ap)     esp_wifi_set_mode(WIFI_MODE_AP);
    else if(sta)    esp_wifi_set_mode(WIFI_MODE_STA);
    else            esp_wifi_set_mode(WIFI_MODE_NULL);

    if(ap){
        esp_wifi_set_config(WIFI_IF_AP, &config_ap);
        tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP);
    }

    if(sta){
        esp_wifi_set_config(WIFI_IF_STA, &config_sta);
        if(!sta_ip.empty() && !sta_subnet.empty() && !sta_gateway.empty() && !sta_dns.empty()){
            tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
            tcpip_adapter_ip_info_t ip_info;
            ip4addr_aton(sta_ip.c_str(), &ip_info.ip);
            ip4addr_aton(sta_gateway.c_str(), &ip_info.gw);
            ip4addr_aton(sta_subnet.c_str(),   &ip_info.netmask);
            tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
            tcpip_adapter_dns_info_t dns_info;
            ipaddr_aton(sta_dns.c_str(),&dns_info.ip);
            tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns_info);
        }
        tk->status[STATUS_BIT_NETWORK] = false;
    }

    mdns_init();
    mdns_hostname_set(_this->tk->hostname.c_str());
    mdns_instance_name_set(_this->tk->hostname.c_str());
    mdns_service_add("Telnet Server ESP32", "_telnet", "_tcp", 23, NULL, 0);

    esp_wifi_start();

    ESP_LOGI(TAG, "COMMIT FINISHED");

};

esp_err_t Network::wifi_event_handler(void *ctx, system_event_t *event)
{ 
    Network* _this = (Network*)ctx;
    switch (event->event_id) {
        case SYSTEM_EVENT_AP_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_START");
            _this->tk->events.emit("SYSTEM_EVENT_AP_START");
            tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, _this->tk->hostname.c_str());
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED");
            _this->tk->events.emit("SYSTEM_EVENT_AP_STACONNECTED");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STAIPASSIGNED");
            _this->tk->events.emit("SYSTEM_EVENT_AP_STAIPASSIGNED");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED");
            _this->tk->events.emit("SYSTEM_EVENT_AP_STADISCONNECTED");
            break;
        case SYSTEM_EVENT_AP_STOP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STOP");
            _this->tk->events.emit("SYSTEM_EVENT_AP_STOP");
            break;

        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            _this->tk->events.emit("SYSTEM_EVENT_STA_START");
            tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, _this->tk->hostname.c_str());
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
            _this->tk->events.emit("SYSTEM_EVENT_STA_CONNECTED");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            _this->tk->events.emit("SYSTEM_EVENT_STA_GOT_IP");
            _this->tk->status[STATUS_BIT_NETWORK] = true;
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
            _this->tk->events.emit("SYSTEM_EVENT_STA_LOST_IP");
            if(_this->sta_enable)_this->tk->status[STATUS_BIT_NETWORK] = false;
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            _this->tk->events.emit("SYSTEM_EVENT_STA_DISCONNECTED");
            if(_this->sta_enable)_this->tk->status[STATUS_BIT_NETWORK] = false;
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_STOP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");
            _this->tk->events.emit("SYSTEM_EVENT_STA_STOP");
            if(_this->sta_enable)_this->tk->status[STATUS_BIT_NETWORK] = false;
            break;
        default:
            break;
    }
    mdns_handle_system_event(ctx, event);
    return ESP_OK;
}

inline int16_t Network::calcRSSI(int32_t r){
    return min(max(2 * (r + 100.0), 0.0), 100.0);
};

void Network::getApIpStr(char* buf){
    tcpip_adapter_ip_info_t ipInfo; 
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo);
    sprintf(buf, IPSTR, IP2STR(&ipInfo.ip));
}

void Network::getStaIpStr(char* buf){
    tcpip_adapter_ip_info_t ipInfo; 
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);
    sprintf(buf, IPSTR, IP2STR(&ipInfo.ip));
}

#endif