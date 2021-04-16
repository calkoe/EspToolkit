#if defined ESP32

#include <Mqtt.h>
 
Mqtt::Mqtt(EspToolkit* tk):tk{tk}{

    // Commit on WiFi Connected
    tk->events.on(0,"SYSTEM_EVENT_STA_GOT_IP",[](void* ctx, void* arg){
        Mqtt* _this = (Mqtt*)ctx;
        _this->commit();
    },this);

    tk->variableAdd("mqtt/enable",  enable,   "📡  MQTT Enable");
    tk->variableAdd("mqtt/uri",     uri,      "📡  MQTT URI");

    tk->commandAdd("mqttCommit",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[128];
        Mqtt*     mqtt = (Mqtt*) c;
        EspToolkit* tk = (EspToolkit*) mqtt->tk;
        if(parCnt>=2){
            snprintf(OUT,128,"Set  mqtt/enable: %s\r\n","true");
            tk->variableSet("mqtt/enable",(char*)"1");
            snprintf(OUT,128,"Set  mqtt/uri: %s\r\n","true");
            tk->variableSet("mqtt/uri",param[1]);
        };
        tk->variableLoad(true);
        mqtt->commit();
        reply((char*)"DONE! ✅ > Type 'mqttStatus' to check status\r\n");
    }, this, "📡 [mqtt/uri] | Apply mqtt settings and connect to configured mqtt server",false);

    tk->commandAdd("mqttPublish",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        Mqtt*     mqtt = (Mqtt*) c;
        //EspToolkit* tk = (EspToolkit*) mqtt->tk;
        if(parCnt>=3){
            mqtt->publish(param[1],param[2]);
            reply((char*)"📨 Message sent!\r\n");
        };
    },this,"📡 [topic] [message] | publish a message to topic",false);

    tk->commandAdd("mqttStatus",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[128];
        Mqtt*     mqtt = (Mqtt*) c;
        EspToolkit* tk = (EspToolkit*) mqtt->tk;
        snprintf(OUT,128,"%-30s : %s\r\n","Enable",mqtt->enable ? "true" : "false");reply(OUT);
        snprintf(OUT,128,"%-30s : %s\r\n","Connected",tk->status[STATUS_BIT_MQTT]  ? "true" : "false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_last_esp_err",esp_err_to_name(mqtt->lastError.esp_tls_last_esp_err));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_stack_err",strerror(mqtt->lastError.esp_tls_stack_err));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_cert_verify_flags",strerror(mqtt->lastError.esp_tls_cert_verify_flags));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_transport_sock_errno",strerror(mqtt->lastError.esp_transport_sock_errno));reply(OUT);
        char* error_type; 
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_NONE) error_type = (char*)"MQTT_ERROR_TYPE_NONE";
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) error_type = (char*)"MQTT_ERROR_TYPE_TCP_TRANSPORT";
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) error_type = (char*)"MQTT_ERROR_TYPE_CONNECTION_REFUSED";
        snprintf(OUT,LONG,"%-30s : %s","error_type",error_type);reply(OUT);
        char* connect_return_code;
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_ACCEPTED) connect_return_code = (char*)"MQTT_CONNECTION_ACCEPTED";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_PROTOCOL) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_PROTOCOL";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_ID_REJECTED) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_ID_REJECTED";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_BAD_USERNAME";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED";
        snprintf(OUT,LONG,"%-30s : %s","connect_return_code",connect_return_code);
        reply("\r\n\r\nSubscriptions:\r\n");
        snprintf(OUT,LONG, "%-30s : %s\r\n","NAME","QOS");reply(OUT);
        for (auto const& x : mqtt->subscriptions){
            snprintf(OUT,LONG, "%-30s : %d\r\n",x.first.c_str(),x.second);reply(OUT);
        }
    }, this, "📡 Shows MQTT connection Status",false);

}

void Mqtt::commit(){   

    tk->status[STATUS_BIT_MQTT] = true;

    if(client){
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    if(enable){
        tk->status[STATUS_BIT_MQTT] = false;
        config.event_handle = &mqtt_event_handler; 
        config.user_context = this;  
        config.uri          = uri.c_str();
        config.client_id    = tk->hostname.empty() ? "EspToolkit" : tk->hostname.c_str();
        client = esp_mqtt_client_init(&config);
        esp_mqtt_client_start(client);
    } 

}

esp_err_t Mqtt::mqtt_event_handler(esp_mqtt_event_handle_t event){
    Mqtt* _this = (Mqtt*)event->user_context;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_BEFORE_CONNECT");
            _this->tk->events.emit("MQTT_EVENT_BEFORE_CONNECT");
            if(_this->enable)_this->tk->status[STATUS_BIT_MQTT] = false;
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_CONNECTED");
            _this->tk->events.emit("MQTT_EVENT_CONNECTED");
            _this->tk->status[STATUS_BIT_MQTT] = true;
            for (auto const& x : _this->subscriptions){
                esp_mqtt_client_subscribe(_this->client, x.first.c_str(), x.second);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_DISCONNECTED");
            _this->tk->events.emit("MQTT_EVENT_DISCONNECTED");
            if(_this->enable)_this->tk->status[STATUS_BIT_MQTT] = false;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_SUBSCRIBED");
            _this->tk->events.emit("MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_UNSUBSCRIBED");
            _this->tk->events.emit("MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_PUBLISHED");
            _this->tk->events.emit("MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:{
            //std::cout << "event->topic_len: " << event->topic_len << std::endl;
            //std::cout << "event->data_len: "  << event->data_len  << std::endl;
            //printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            //printf("DATA=%.*s\r\n", event->data_len, event->data);
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_DATA");
            _this->tk->events.emit("MQTT_EVENT_DATA");
            char* topic = (char*) malloc(event->topic_len + 1);
            char* data  = (char*) malloc(event->data_len  + 1);
            if(event->topic_len > 0) xthal_memcpy(topic, event->topic, event->topic_len + 1);
            if(event->data_len  > 0) xthal_memcpy(data,  event->data,  event->data_len  + 1);
            topic[event->topic_len] = '\0';
            data[event->data_len]   = '\0';
            _this->tk->events.emit(topic,(void*)data,event->data_len  + 1);
            delete topic;
            delete data;
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_ERROR");
            if(_this->enable)_this->tk->status[STATUS_BIT_MQTT] = false;
            _this->lastError = *event->error_handle;
            _this->tk->events.emit("MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_OTHER id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

void Mqtt::publish(std::string topic, const char* message, int qos, int retain){
    if(qos < 0 || qos > 2) return;
    if(retain < 0 || retain > 1) return;
    if(client) esp_mqtt_client_publish(client, topic.c_str(), message, strlen(message), qos, retain);
};

void Mqtt::subscribe(std::string topic, int qos){
    if(qos < 0 || qos > 2) return;
    subscriptions.insert({topic,qos});
    if(client) esp_mqtt_client_subscribe(client, topic.c_str(), qos);
};

void Mqtt::unsubscribe(std::string topic){
    auto it = subscriptions.find(topic);
    if(it != subscriptions.end()) subscriptions.erase (it);
    if(client) esp_mqtt_client_unsubscribe(client, topic.c_str());
}; 

#endif