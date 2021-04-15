#if defined ESP32

#include <Network.h>

Network::Network(EspToolkit* tk):tk{tk}{

    // TCPIP
    tcpip_adapter_init();

    // TELNET
    if(telnet_enable){
        telnet = new Telnet(&(this->tk->events),(char*)EVT_TK_COMMAND,(char*)EVT_TK_BROADCAST);
    }

    // BUTTON TOGGLE AP
    tk->button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,1000,(char*)"bootbutton1000ms");
    tk->events.on(0,"bootbutton1000ms",[](void* ctx, void* arg){
        Network*    network = (Network*) ctx;
        EspToolkit* tk      = (EspToolkit*) network->tk;
        if(!*(bool*)arg){
            ESP_LOGE(TAG, "TOGGLE AP");
            network->ap_enable = !network->ap_enable;
            tk->variableLoad(true);
            esp_restart();
        }
    },this);

    tk->variableAdd("wifi/enable",       sta_enable,        "📶 Enable WiFi STA-Mode");
    tk->variableAdd("wifi/network",      sta_network,       "📶 Network SSID");
    tk->variableAdd("wifi/password",     sta_password,      "📶 Network Password");
    tk->variableAdd("wifi/ip",           sta_ip,            "📶 IP (leave blank to use DHCP)");
    tk->variableAdd("wifi/subnet",       sta_subnet,        "📶 Subnet");
    tk->variableAdd("wifi/gateway",      sta_gateway,       "📶 Gateway");
    tk->variableAdd("wifi/dns",          sta_dns,           "📶 DNS");
    tk->variableAdd("wifi/sleep",        sta_modenSleep,    "📶 Enable modem sleep to save energy");
    tk->variableAdd("hotspot/enable",    ap_enable,         "📶 Enable WiFi Hotspot-Mode");
    tk->variableAdd("hotspot/password",  ap_password,       "📶 Hotspot Password");
    tk->variableAdd("telnet/enable",     telnet_enable,     "📶 Enables Telnet Server on Port 23");
    
    tk->commandAdd("wifiStatus",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        Network*    network = (Network*) c;
        EspToolkit* tk      = (EspToolkit*) network->tk;
        char OUT[LONG];
        reply((char*)"📶 Newtwork:\r\n");
        const char* s;
        switch(WiFi.status()){
            case WL_CONNECTED:      s = "WL_CONNECTED";break;
            case WL_NO_SHIELD:      s = "WL_NO_SHIELD";break;
            case WL_IDLE_STATUS:    s = "WL_IDLE_STATUS";break;
            case WL_CONNECT_FAILED: s = "WL_CONNECT_FAILED";break;
            case WL_NO_SSID_AVAIL:  s = "WL_NO_SSID_AVAIL";break;
            case WL_SCAN_COMPLETED: s = "WL_SCAN_COMPLETED";break;
            case WL_CONNECTION_LOST:s = "WL_CONNECTION_LOST";break;
            case WL_DISCONNECTED:   s = "WL_DISCONNECTED";break;
            default: s = "UNKOWN";
        };
        const char* m;
        switch(WiFi.getMode()){
            case WIFI_AP:           m = "WIFI_AP";break;
            case WIFI_STA:          m = "WIFI_STA";break;
            case WIFI_AP_STA:       m = "WIFI_AP_STA";break;
            case WIFI_OFF:          m = "WIFI_OFF";break;
            default:                m = "UNKOWN";
        };
        IPAddress localIP       = WiFi.localIP();
        IPAddress subnetMask    = WiFi.subnetMask();
        IPAddress gatewayIP     = WiFi.gatewayIP();
        IPAddress apIP          = WiFi.softAPIP();
        snprintf(OUT,LONG,"%-30s : %s","Mode",m);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Status",s);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Connected",tk->status[STATUS_BIT_NETWORK]?"true":"false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Hostname",tk->hostname.c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","LocalMAC",WiFi.macAddress().c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","BSSID",WiFi.BSSIDstr().c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","Channel",WiFi.channel());reply(OUT);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","LocalIP",localIP[0],localIP[1],localIP[2],localIP[3]);reply(OUT);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","SubnetMask",subnetMask[0],subnetMask[1],subnetMask[2],subnetMask[3]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","GatewayIP",gatewayIP[0],gatewayIP[1],gatewayIP[2],gatewayIP[3]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d dBm (%d%%)\r\n","RSSI",WiFi.RSSI(),network->calcRSSI(WiFi.RSSI()));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","AP MAC",WiFi.softAPmacAddress().c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d.%d.%d.%d\r\n","AP IP",apIP[0],apIP[1],apIP[2],apIP[3]);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d\r\n","AP Stations",WiFi.softAPgetStationNum());reply(OUT);
        if(network->telnet) snprintf(OUT,LONG,"%-30s : %s\r\n","Telnet connected",network->telnet->clientSock > 0 ? "true" : "false");
    },this,  "📶 Shows System / Wifi status");
    
    /*
    tk->commandAdd("wifiOta",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        Network*    network = (Network*) c;
        EspToolkit* tk      = (EspToolkit*) network->tk;
        char OUT[LONG];

        if(parCnt==2){


            FILE* f = fopen("/spiffs/localhost.pem", "r");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                return;
            }
            char* line = new char[5000];
            fgets(line, sizeof(line), f);
            fclose(f);

            esp_http_client_config_t config = {};
            config.url = param[1];
            config.cert_pem = line;
            config.skip_cert_common_name_check=true;
            esp_err_t ret = esp_https_ota(&config);
            if (ret == ESP_OK) {
                esp_restart();
            } else {
                reply(strerror(ret));reply(" - Firmware upgrade failed\r\n");
            }

        }else{
            tk->commandMan("wifiOta",reply);
        }
    },this,"📶 [url] | load and install new firmware from URL (https only!)");
    */
   
    /*tk->commandAdd("wifiScan",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    network = (Network*) c;
        EspToolkit* tk      = (EspToolkit*) network->tk;
        reply((char*)"Scaning for Networks...");
        reply((char*)tk->EOL);
        uint8_t n = WiFi.scanNetworks();
        if(n){
            for (uint8_t i = 0; i < n; i++){
                const char* e;
                switch(WiFi.encryptionType(i)){
                    case 0: e = "OPEN";break;
                    case 1: e = "WEP";break;
                    case 2: e = "WPA_PSK";break;
                    case 3: e = "WPA2_PSK";break;
                    case 4: e = "WPA_WPA2_PSK";break;
                    case 5: e = "WPA2_ENTERPRISE";break;
                    case 6: e = "AUTH_MAX";break;
                    default:e = "UNKOWN";
                }
                snprintf(OUT,LONG,"%-30s : %d dBm (%d%%) (%s) BSSID: %s %s",WiFi.SSID(i).c_str(),WiFi.RSSI(i), network->calcRSSI(WiFi.RSSI(i)),e,WiFi.BSSIDstr(i).c_str(),tk->EOL);reply(OUT);
            }
        }else reply((char*)"❌ No Networks found!");
    },this,    "📶 Scans for nearby networks");*/

    tk->commandAdd("wifiCommit",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    network = (Network*) c;
        EspToolkit* tk      = (EspToolkit*) network->tk;
        if(parCnt>=2){
                snprintf(OUT,LONG,"Set  wifi/enabled: %s\r\n","true");reply(OUT);
                network->sta_enable  = true;
                snprintf(OUT,LONG,"Set  wifi/network: %s\r\n",param[1]);reply(OUT);
                tk->variableSet("wifi/network",param[1]);
        };
        if(parCnt>=3){
                snprintf(OUT,LONG,"Set wifi/password: %s\r\n",param[2]);reply(OUT);
                tk->variableSet("wifi/password",param[2]);
        };
        tk->variableLoad(true);
        reply((char*)"DONE! ✅ > Type 'wifiStatus' to check status\r\n");
        network->commit();

    },this, "📶 [network] [password] | apply network settings and connect to configured network",false);

    tk->commandAdd("wifiDns",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[LONG];
        Network*    network = (Network*) c;
        //EspToolkit* tk      = (EspToolkit*) network->tk;
        if(parCnt==2){
            IPAddress remote_addr;
            if(WiFi.hostByName(param[1], remote_addr)){
                snprintf(OUT,LONG,"✅ DNS %s -> %d.%d.%d.%d\r\n",param[1],remote_addr[0],remote_addr[1],remote_addr[2],remote_addr[3]);reply(OUT);
            }else{
                snprintf(OUT,LONG,"❌ DNS lookup failed\r\n");reply(OUT);
            };
        }
    },this,  "📶 [ip] | check internet connection");
};

void Network::commit(){

    tk->status[STATUS_BIT_NETWORK] = true;

    esp_event_loop_init(wifi_event_handler, this);
    WiFi.persistent(false); 
    WiFi.mode(WIFI_STA);
    WiFi.setHostname((const char*)tk->hostname.c_str()); 
    WiFi.setSleep(sta_modenSleep);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.mode(WIFI_OFF);

    //STA
    if(sta_enable && sta_network.length()){
        
        tk->status[STATUS_BIT_NETWORK] = false;

        if(sta_ip.length() && sta_subnet.length() && sta_gateway.length() && sta_dns.length()){
            IPAddress wifiIp;
            IPAddress wifiDns;
            IPAddress wifiGateway;
            IPAddress wifiSubnet; 
            wifiIp.fromString((String)sta_ip.c_str());
            wifiDns.fromString((String)sta_dns.c_str());
            wifiSubnet.fromString((String)sta_subnet.c_str());
            wifiGateway.fromString((String)sta_gateway.c_str());
            WiFi.config(wifiIp,wifiGateway,wifiSubnet,wifiDns);
        }
        WiFi.begin((const char*)sta_network.c_str(),(const char*)sta_password.c_str());
    };

    //AP
    if(ap_enable && tk->hostname.length() && !WiFi.softAPgetStationNum()){
        WiFi.softAPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));
        WiFi.softAP((const char*)tk->hostname.c_str(),(const char*)ap_password.c_str());
    };

};

esp_err_t Network::wifi_event_handler(void *ctx, system_event_t *event)
{ 
    Network* _this = (Network*)ctx;
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            _this->tk->events.emit("SYSTEM_EVENT_STA_START");
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