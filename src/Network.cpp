#include <Network.h>

Network* Network::_this{nullptr};

Network::Network(){
    if(_this) return;
    _this = this;

    // NETIF
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,  &network_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   ESP_EVENT_ANY_ID,  &network_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT,  ESP_EVENT_ANY_ID,  &network_event_handler, this));

    // SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // AP BUTTON TOGGLE
    EspToolkitInstance->button.add((gpio_num_t)BOOTBUTTON_PIN,GPIO_FLOATING,1000,(char*)"bootbutton1000ms");
    EspToolkitInstance->events.on(0,"bootbutton1000ms",[](void* ctx, void* arg){
        Network*    _this = (Network*) ctx;
        if(!*(bool*)arg){
            ESP_LOGI("NETWORK", "TOGGLE AP");
            _this->hotspot_enable = !_this->hotspot_enable;
            EspToolkitInstance->variableLoad(true);
            _this->commit();
        }
    },this);

    // AP Autostart
    EspToolkitInstance->timer.setInterval([](void* ctx){
        Network*    _this = (Network*) ctx;
        if(_this->hotspot_autostart){
            if(!_this->hotspot_enable && !_this->hotspot_autostart_triggered && (!_this->wifi_enable || !EspToolkitInstance->status[STATUS_BIT_NETWORK])){
            _this->hotspot_autostart_triggered = true;
            _this->hotspot_enable = true;
            _this->commit();
            }else if(_this->hotspot_enable && _this->hotspot_autostart_triggered && _this->wifi_enable && EspToolkitInstance->status[STATUS_BIT_NETWORK]){
                _this->hotspot_autostart_triggered = false;
                _this->hotspot_enable = false;
                _this->commit();
            }
        }
        
    },this,10000,"hotspot autostart");

    // CONFIG VARIABLES
    EspToolkitInstance->variableAdd("hotspot/enable",    hotspot_enable,    "📶 Enable WiFi Hotspot");
    EspToolkitInstance->variableAdd("hotspot/autostart", hotspot_autostart, "📶 Start Hotspot if threre is no STA connection aviable");
    EspToolkitInstance->variableAdd("wifi/enable",       wifi_enable,       "📶 Enable WiFi STA");
    EspToolkitInstance->variableAdd("wifi/network",      wifi_network,      "📶 Network SSID");
    EspToolkitInstance->variableAdd("wifi/password",     wifi_password,     "📶 Network Password");
    EspToolkitInstance->variableAdd("wifi/powerSafe",    wifi_powerSave,    "📶 WiFI Power Safe: 0=WIFI_PS_NONE | 1=WIFI_PS_MIN_MODEM | 2=WIFI_PS_MAX_MODEM");
    EspToolkitInstance->variableAdd("eth/enable",        eth_enable,        "📶 Enable ETH Interface");
    EspToolkitInstance->variableAdd("net/ip",            ip,                "📶 Static IP      (leave blank to use DHCP)");
    EspToolkitInstance->variableAdd("net/subnet",        subnet,            "📶 Static Subnet  (leave blank to use DHCP)");
    EspToolkitInstance->variableAdd("net/gateway",       gateway,           "📶 Static Gateway (leave blank to use DHCP)");
    EspToolkitInstance->variableAdd("net/dns",           dns,               "📶 Static DNS     (leave blank to use DHCP)");
    EspToolkitInstance->variableAdd("net/sntp",          sntp,              "📶 Static SNTP Server for time synchronisation (leave blank to disable)");
    EspToolkitInstance->variableAdd("telnet/enable",     telnet_enable,     "📶 Enables Telnet Server on Port 23");
    EspToolkitInstance->variableAdd("ota/caCert",        ota_caCert,        "📶 CA Certificate for OTA Updates");

    // COMMANDS
    EspToolkitInstance->commandAdd("netStatus",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        Network*    _this   = (Network*) ctx;
        char OUT[LONG];
        reply((char*)"📶 Network:\r\n");
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
        esp_netif_ip_info_t ipInfoAp, ipInfoSta, ipInfoEth;
        esp_netif_get_ip_info(_this->netif_hotspot, &ipInfoAp);
        esp_netif_get_ip_info(_this->netif_wifi, &ipInfoSta);
        esp_netif_get_ip_info(_this->netif_eth, &ipInfoEth);
        uint8_t macAp[6], macSta[6];
        esp_wifi_get_mac(WIFI_IF_AP, macAp);
        esp_wifi_get_mac(WIFI_IF_STA, macSta);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Hostname",EspToolkitInstance->hostname.c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Ready",EspToolkitInstance->status[STATUS_BIT_NETWORK]?"true":"false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","WIFI Mode",m);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP IP",IP2STR(&ipInfoAp.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP MASK",IP2STR(&ipInfoAp.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP GW",IP2STR(&ipInfoAp.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "AP MAC", macAp[0], macAp[1], macAp[2], macAp[3], macAp[4], macAp[5]);reply(OUT);
        wifi_sta_list_t clients;
        esp_wifi_ap_get_sta_list(&clients); 
        snprintf(OUT,LONG,"%-30s : %d\r\n","AP Stations",clients.num);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA IP",IP2STR(&ipInfoSta.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA MASK",IP2STR(&ipInfoSta.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA GW",IP2STR(&ipInfoSta.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","STA DNS",IP2STR(&dns_getserver(0)->u_addr.ip4));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n", "STA MAC", macSta[0], macSta[1], macSta[2], macSta[3], macSta[4], macSta[5]);reply(OUT);
        wifi_ap_record_t infoSta;
        esp_wifi_sta_get_ap_info(&infoSta);
        snprintf(OUT,LONG,"%-30s : %02X:%02X:%02X:%02X:%02X:%02X\r\n","STA BSSID",infoSta.bssid[0], infoSta.bssid[1], infoSta.bssid[2], infoSta.bssid[3], infoSta.bssid[4], infoSta.bssid[5]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d dBm (%d%%)\r\n","STA RSSI",infoSta.rssi,_this->calcRSSI(infoSta.rssi));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","STA Primary channel",infoSta.primary);reply(OUT);
        const char* wifi_second_chan;
        if(infoSta.second == WIFI_SECOND_CHAN_NONE)  wifi_second_chan = "WIFI_SECOND_CHAN_NONE";
        if(infoSta.second == WIFI_SECOND_CHAN_ABOVE) wifi_second_chan = "WIFI_SECOND_CHAN_ABOVE";
        if(infoSta.second == WIFI_SECOND_CHAN_BELOW) wifi_second_chan = "WIFI_SECOND_CHAN_BELOW";
        snprintf(OUT,LONG,"%-30s : %s\r\n","STA Secondary channel",wifi_second_chan);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","ETH IP",IP2STR(&ipInfoEth.ip));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","ETH MASK",IP2STR(&ipInfoEth.netmask));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","ETH GW",IP2STR(&ipInfoEth.gw));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","ETH DNS",IP2STR(&dns_getserver(0)->u_addr.ip4));reply(OUT);
        if(_this->telnet) snprintf(OUT,LONG,"%-30s : %s\r\n","Telnet connected",_this->telnet->clientSock > 0 ? "true" : "false");reply(OUT);
        sntp_sync_status_t sntp_sync_status = sntp_get_sync_status();
        snprintf(OUT,LONG,"%-30s : %s\r\n","SNTP Time Sync",sntp_sync_status == SNTP_SYNC_STATUS_COMPLETED ? "true" : "false");reply(OUT);
    },this,  "📶 Wifi status");
    
    EspToolkitInstance->commandAdd("netOta",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
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
            EspToolkitInstance->commandMan("netOta",reply);
        }
    },this,"📶 [url] | load and install new firmware from URL (https only!)");

    EspToolkitInstance->commandAdd("wifiScan",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if(mode == WIFI_MODE_NULL){esp_wifi_set_mode(WIFI_MODE_STA);}
        uint16_t number = NETWORK_DEFAULT_SCAN_LIST_SIZE;
        wifi_ap_record_t ap_info[NETWORK_DEFAULT_SCAN_LIST_SIZE];
        uint16_t ap_count = 0;
        memset(ap_info, 0, sizeof(ap_info));
        wifi_scan_config_t config{};
        reply((char*)"Scaning Networks...\r\n");
        esp_err_t scanResult = esp_wifi_scan_start(&config, true);
        if(scanResult == ESP_OK) {
            esp_wifi_scan_get_ap_records(&number, ap_info);
            esp_wifi_scan_get_ap_num(&ap_count);
            for (int i = 0; (i < NETWORK_DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
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
                    case WIFI_AUTH_WPA3_PSK:        authmode = (char*)"WIFI_AUTH_WPA3_PSK";break;
                    case WIFI_AUTH_WPA2_WPA3_PSK:   authmode = (char*)"WIFI_AUTH_WPA2_WPA3_PSK";break;
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

    EspToolkitInstance->commandAdd("netCommit",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
        if(parCnt>=2){
                snprintf(OUT,LONG,"Set  wifi/enabled: %s\r\n","true");reply(OUT);
                _this->wifi_enable  = true;
                snprintf(OUT,LONG,"Set  wifi/network: %s\r\n",param[1]);reply(OUT);
                _this->wifi_network = param[1];
        };
        if(parCnt>=3){
                snprintf(OUT,LONG,"Set wifi/password: %s\r\n",param[2]);reply(OUT);
                _this->wifi_password = param[2];
        };
        EspToolkitInstance->variableLoad(true);
        reply((char*)"DONE! ✅ > Type 'netStatus' to check status\r\n");
        _this->commit();

    },this, "📶 [network] [password] | apply network settings and connect to configured network",false);
    
    EspToolkitInstance->commandAdd("netDns",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    _this = (Network*) ctx;
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
            EspToolkitInstance->commandMan("netDns",reply);
        }
    },this,  "📶 [ip] | resolve DNS");

};

void Network::commit(){

    EspToolkitInstance->status[STATUS_BIT_NETWORK] = true;

    // CONFIG
    bool wifi    = wifi_enable     && !wifi_network.empty();
    bool hotspot = hotspot_enable  && !EspToolkitInstance->hostname.empty();
    bool eth     = eth_enable      && !EspToolkitInstance->hostname.empty();
    EspToolkitInstance->hostname.copy((char*)config_ap.ap.ssid,32,0);
    EspToolkitInstance->password.copy((char*)config_ap.ap.password,64,0);
    wifi_network.copy((char*)config_sta.sta.ssid,32,0);
    wifi_password.copy((char*)config_sta.sta.password,64,0);

    // CONFIG INTERFACES
    if(hotspot && !netif_hotspot){
        netif_hotspot = esp_netif_create_default_wifi_ap();
    }
    if(wifi && !netif_wifi){
        netif_wifi = esp_netif_create_default_wifi_sta();
    }

    // SETUP WIFI
    wifi_init_config_t config_init = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config_init);
    esp_wifi_set_ps((wifi_ps_type_t)wifi_powerSave);

    // SET WIFI MODE
    if(wifi && hotspot)   esp_wifi_set_mode(WIFI_MODE_APSTA);
    else if(hotspot)     esp_wifi_set_mode(WIFI_MODE_AP);
    else if(wifi)    esp_wifi_set_mode(WIFI_MODE_STA);
    else            esp_wifi_set_mode(WIFI_MODE_NULL);

    // NETIF AP
    wifi_sta_list_t clients;
    esp_wifi_ap_get_sta_list(&clients); 
    if(hotspot && !clients.num){
        esp_wifi_set_config(WIFI_IF_AP, &config_ap);
    }

    // NETIF STA
    if(wifi){
        esp_wifi_set_config(WIFI_IF_STA, &config_sta);
        if(!ip.empty() && !subnet.empty() && !gateway.empty() && !dns.empty()){
            esp_netif_dhcpc_stop(netif_wifi);
            esp_netif_ip_info_t ip_info;
            ip_info.ip.addr       = static_cast<uint32_t>(ipaddr_addr(ip.c_str()));
            ip_info.gw.addr       = static_cast<uint32_t>(ipaddr_addr(gateway.c_str()));
            ip_info.netmask.addr  = static_cast<uint32_t>(ipaddr_addr(subnet.c_str()));
            esp_netif_set_ip_info(netif_wifi,&ip_info);
            esp_netif_dns_info_t dns_info;
	        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
	        dns_info.ip.u_addr.ip4.addr = static_cast<uint32_t>(ipaddr_addr(dns.c_str()));
            esp_netif_set_dns_info(netif_wifi,ESP_NETIF_DNS_MAIN,&dns_info);
        }
    }

    // NETIF ETH
    if(eth){
        if(!netif_eth){
            esp_netif_config_t netif_eth_cfg = ESP_NETIF_DEFAULT_ETH();
            netif_eth = esp_netif_new((&netif_eth_cfg));
            esp_eth_set_default_handlers(netif_eth);
            eth_mac_config_t mac_config  = ETH_MAC_DEFAULT_CONFIG();
            esp_eth_mac_t *mac           = esp_eth_mac_new_esp32(&mac_config);
            eth_phy_config_t phy_config  = ETH_PHY_DEFAULT_CONFIG();
            phy_config.phy_addr          = 1;
            esp_eth_phy_t *phy           = esp_eth_phy_new_lan8720(&phy_config);
            esp_eth_config_t config      = ETH_DEFAULT_CONFIG(mac, phy);
            ESP_ERROR_CHECK(esp_eth_driver_install(&config, &netif_eth_handle));
            ESP_ERROR_CHECK(esp_netif_attach(netif_eth, esp_eth_new_netif_glue(netif_eth_handle)));
        }
        if(!ip.empty() && !subnet.empty() && !gateway.empty() && !dns.empty()){
            esp_netif_dhcpc_stop(netif_eth);
            esp_netif_ip_info_t ip_info;
            ip_info.ip.addr       = static_cast<uint32_t>(ipaddr_addr(ip.c_str()));
            ip_info.gw.addr       = static_cast<uint32_t>(ipaddr_addr(gateway.c_str()));
            ip_info.netmask.addr  = static_cast<uint32_t>(ipaddr_addr(subnet.c_str()));
            esp_netif_set_ip_info(netif_eth,&ip_info);
            esp_netif_dns_info_t dns_info;
	        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
	        dns_info.ip.u_addr.ip4.addr = static_cast<uint32_t>(ipaddr_addr(dns.c_str()));
            esp_netif_set_dns_info(netif_eth,ESP_NETIF_DNS_MAIN,&dns_info);
        }else{
            esp_netif_dhcpc_start(netif_eth);
        }
    }else if(netif_eth){
        esp_netif_dhcpc_stop(netif_eth);
    }

    // TELNET
    if(telnet_enable){
        if(!telnet){
            telnet = new Telnet;
        }
    }else if(telnet){
        delete telnet;
    }

    // MDNS
    mdns_init();
    mdns_hostname_set(EspToolkitInstance->hostname.c_str());
    mdns_instance_name_set(EspToolkitInstance->hostname.c_str());
    mdns_service_add("Telnet Server ESP32", "_telnet", "_tcp", 23, NULL, 0);

    // SNTP
    if(!sntp.empty()){
        sntp_setservername(0, (char*)_this->sntp.c_str());
        sntp_set_sync_interval(10 * 60 * 1000);
        sntp_init();
    }else{
        sntp_stop();
    }

    // START WIFI
    if(hotspot || wifi){
        esp_wifi_start();
    }else{
        esp_wifi_stop();
    }

    // START ETHERNET
    if(eth && netif_eth){
        esp_eth_start(netif_eth_handle);
    }else if(netif_eth){
        esp_eth_stop(netif_eth_handle);
    }

    ESP_LOGI("NETWORK", "COMMIT FINISHED");

};

void Network::network_event_handler(void* ctx, esp_event_base_t event_base, int32_t event_id, void* event_data){ 
    Network* _this = (Network*)ctx;
    if(event_base == WIFI_EVENT){
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI("NETWORK", "WIFI_EVENT_AP_START");
                EspToolkitInstance->events.emit("WIFI_EVENT_AP_START");
                esp_netif_set_hostname(_this->netif_hotspot,EspToolkitInstance->hostname.c_str());
                break;
            case SYSTEM_EVENT_AP_STACONNECTED:
                ESP_LOGI("NETWORK", "SYSTEM_EVENT_AP_STACONNECTED");
                EspToolkitInstance->events.emit("SYSTEM_EVENT_AP_STACONNECTED");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STACONNECTED");
                EspToolkitInstance->events.emit("WIFI_EVENT_AP_STACONNECTED");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STADISCONNECTED");
                EspToolkitInstance->events.emit("WIFI_EVENT_AP_STADISCONNECTED");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI("NETWORK", "WIFI_EVENT_AP_STOP");
                EspToolkitInstance->events.emit("WIFI_EVENT_AP_STOP");
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI("NETWORK", "WIFI_EVENT_STA_START");
                EspToolkitInstance->events.emit("WIFI_EVENT_STA_START");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                esp_netif_set_hostname(_this->netif_wifi,EspToolkitInstance->hostname.c_str());
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI("NETWORK", "WIFI_EVENT_STA_CONNECTED");
                EspToolkitInstance->events.emit("WIFI_EVENT_STA_CONNECTED");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = true;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI("NETWORK", "WIFI_EVENT_STA_DISCONNECTED");
                EspToolkitInstance->events.emit("WIFI_EVENT_STA_DISCONNECTED");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI("NETWORK", "WIFI_EVENT_STA_STOP");
                EspToolkitInstance->events.emit("WIFI_EVENT_STA_STOP");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                break;
        }
    }

    if(event_base == ETH_EVENT){
        switch (event_id) {
            case ETHERNET_EVENT_START:
                ESP_LOGI("NETWORK", "ETHERNET_EVENT_START");
                EspToolkitInstance->events.emit("ETHERNET_EVENT_START");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                break;
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI("NETWORK", "ETHERNET_EVENT_CONNECTED");
                EspToolkitInstance->events.emit("ETHERNET_EVENT_CONNECTED");
                //uint8_t mac_addr[6]{0};
                //esp_eth_ioctl(_this->netif_eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
                //ESP_LOGI("NETWORK", "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = true;
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI("NETWORK", "ETHERNET_EVENT_DISCONNECTED");
                EspToolkitInstance->events.emit("ETHERNET_EVENT_DISCONNECTED");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI("NETWORK", "ETHERNET_EVENT_STOP");
                EspToolkitInstance->events.emit("ETHERNET_EVENT_STOP");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                break;
        }
    }

    if(event_base == IP_EVENT){
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI("NETWORK", "IP_EVENT_STA_GOT_IP");
                EspToolkitInstance->events.emit("IP_EVENT_STA_GOT_IP");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = true;
                break;
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI("NETWORK", "IP_EVENT_STA_LOST_IP");
                EspToolkitInstance->events.emit("IP_EVENT_STA_LOST_IP");
                EspToolkitInstance->status[STATUS_BIT_NETWORK] = false;
                break;
            case IP_EVENT_ETH_GOT_IP:
                ESP_LOGI("NETWORK", "IP_EVENT_ETH_GOT_IP");
                EspToolkitInstance->events.emit("IP_EVENT_ETH_GOT_IP");

                esp_netif_ip_info_t ipInfoEth;
                esp_netif_get_ip_info(_this->netif_eth, &ipInfoEth);
                char OUT[LONG];
                snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","ETH IP",IP2STR(&ipInfoEth.ip));
                std:: cout << OUT;

                EspToolkitInstance->status[STATUS_BIT_NETWORK] = true;
                break;
        }
    }

}

int16_t Network::calcRSSI(int32_t r){
    return min(max(2 * (r + 100.0), 0.0), 100.0);
};