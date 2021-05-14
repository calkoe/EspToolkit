#include <Mqtt.h>

Mqtt* Mqtt::_this{nullptr};

Mqtt::Mqtt(EspToolkit* tk):tk{tk}{
    if(_this) return;
    _this = this;

    // Commit on WiFi Connected
    tk->events.on(0,"IP_EVENT_STA_GOT_IP",[](void* ctx, void* arg){
        _this->commit();
    },this);

    // EVT_TK_BROADCAST -> SERIAL
    tk->events.on(0,EVT_TK_BROADCAST,[](void* ctx, void* arg){
        print((char*)arg);
    },NULL);

    // Shell
    tk->variableAdd("mqtt/enable",      enable,         "📡  MQTT Enable");
    tk->variableAdd("mqtt/uri",         uri,            "📡  MQTT URI");
    tk->variableAdd("mqtt/cmd",         commandTopic,   "📡  MQTT Receive and send Commands via this topic");

    tk->commandAdd("mqttCommit",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[128];
        if(parCnt>=2){
            snprintf(OUT,128,"Set  mqtt/enable: %s\r\n","true");
            _this->tk->variableSet("mqtt/enable",(char*)"1");
            snprintf(OUT,128,"Set  mqtt/uri: %s\r\n","true");
            _this->tk->variableSet("mqtt/uri",param[1]);
        };
        _this->tk->variableLoad(true);
        _this->commit();
        reply((char*)"DONE! ✅ > Type 'mqttStatus' to check status\r\n");
    }, NULL, "📡 [mqtt/uri] | Apply mqtt settings and connect to configured mqtt server",false);

    tk->commandAdd("mqttPublish",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt>=3){
            _this->publish(param[1],param[2]);
            reply((char*)"📨 Message sent!\r\n");
        };
    },NULL,"📡 [topic] [message] | publish a message to topic",false);

    tk->commandAdd("mqttStatus",[](void* ctx, void (*reply)(const char*), char** param,uint8_t parCnt){
        char OUT[128];
        snprintf(OUT,LONG,"%-30s : %s\r\n","Enable",_this->enable ? "true" : "false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","Connected",_this->tk->status[STATUS_BIT_MQTT]  ? "true" : "false");reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_last_esp_err",esp_err_to_name(_this->lastError.esp_tls_last_esp_err));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_stack_err",strerror(_this->lastError.esp_tls_stack_err));reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","esp_tls_cert_verify_flags",strerror(_this->lastError.esp_tls_cert_verify_flags));reply(OUT);
        char* error_type; 
        if(_this->lastError.error_type == MQTT_ERROR_TYPE_NONE) error_type = (char*)"MQTT_ERROR_TYPE_NONE";
        if(_this->lastError.error_type == MQTT_ERROR_TYPE_ESP_TLS) error_type = (char*)"MQTT_ERROR_TYPE_ESP_TLS";
        if(_this->lastError.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) error_type = (char*)"MQTT_ERROR_TYPE_CONNECTION_REFUSED";
        snprintf(OUT,LONG,"%-30s : %s","error_type",error_type);reply(OUT);
        char* connect_return_code;
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_ACCEPTED) connect_return_code = (char*)"MQTT_CONNECTION_ACCEPTED";
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_PROTOCOL) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_PROTOCOL";
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_ID_REJECTED) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_ID_REJECTED";
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE";
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_BAD_USERNAME";
        if(_this->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) connect_return_code = (char*)"MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED";
        snprintf(OUT,LONG,"%-30s : %s","connect_return_code",connect_return_code);
        reply("\r\n\r\nSubscriptions:\r\n");
        for (auto const& x : _this->subscriptions){
            snprintf(OUT,LONG, "%-30s : %d\r\n",x.first.c_str(),x.second);reply(OUT);
        }
    }, NULL, "📡  MQTT Status",false);

}

void Mqtt::commit(){   

    tk->status[STATUS_BIT_MQTT] = true;

    if(client){
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    if(enable){

        // Config
        tk->status[STATUS_BIT_MQTT] = false;
        config.user_context = this;  
        config.uri          = uri.c_str();
        config.client_id    = tk->hostname.empty() ? "EspToolkit" : tk->hostname.c_str();

        // Load cert_pem from SPIFFS 
        static char* ca_cert{nullptr};
        if(ca_cert) delete[] ca_cert;
        ca_cert = loadFile("/s/ca/mqtt.pem");
        config.skip_cert_common_name_check = true;
        config.cert_pem = ca_cert;

        // Connect
        client = esp_mqtt_client_init(&config);
        esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);
    } 

    ESP_LOGI("MQTT", "COMMIT FINISHED");

}

void Mqtt::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    ESP_LOGD("MQTT", "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = *(esp_mqtt_event_handle_t*)event_data;
    //esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_BEFORE_CONNECT");
            _this->tk->events.emit("MQTT_EVENT_BEFORE_CONNECT");
            if(_this->enable)_this->tk->status[STATUS_BIT_MQTT] = false;
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_CONNECTED");
            _this->tk->events.emit("MQTT_EVENT_CONNECTED");
            _this->tk->status[STATUS_BIT_MQTT] = true;
            if(!_this->commandTopic.empty()) esp_mqtt_client_subscribe(_this->client, _this->commandTopic.c_str(), 2);
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
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_DATA");
            _this->tk->events.emit("MQTT_EVENT_DATA");  

            // Add Zero Termination
            char* topic = (char*) malloc(event->topic_len + 1);
            char* data  = (char*) malloc(event->data_len  + 1);
            snprintf(topic, event->topic_len + 1, "%.*s", event->topic_len, event->topic);
            snprintf(data,  event->data_len + 1, "%.*s", event->data_len, event->data);

            // Commands Topic
            if(!_this->commandTopic.empty() && _this->commandTopic == std::string(topic)){
                simple_cmd_t simple_cmd{
                    strdup(data), 
                    print
                };
                _this->tk->events.emit(EVT_TK_COMMAND,(void*)&simple_cmd,sizeof(simple_cmd_t));
            }

            // Event
            _this->tk->events.emit(topic,(void*)data,event->data_len  + 1);

            // CleanUp
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
}

void Mqtt::publish(std::string topic, const char* message, int qos, int retain){
    if(qos < 0 || qos > 2) return;
    if(retain < 0 || retain > 1) return;
    if(client) esp_mqtt_client_publish(client, topic.c_str(), message, strlen(message), qos, retain);
};

void Mqtt::subscribe(std::string topic, int qos){
    if(qos < 0 || qos > 2) return;
    if(subscriptions.find(topic) == subscriptions.end()){
        subscriptions.insert({topic,qos});
        if(client) esp_mqtt_client_subscribe(client, topic.c_str(), qos);
    }
};

void Mqtt::unsubscribe(std::string topic){
    auto it = subscriptions.find(topic);
    if(it != subscriptions.end()) subscriptions.erase (it);
    if(client) esp_mqtt_client_unsubscribe(client, topic.c_str());
}; 

void Mqtt::print(const char* text){
    if(!_this->commandTopic.empty()) _this->publish(_this->commandTopic + "/reply",text,2,0);
};