#if defined ESP32

#include <Mqtt.h>
 
Mqtt::Mqtt(EspToolkit* tk):tk{tk}{

    tk->status[STATUS_BIT_MQTT] = false;

    // Commit on WiFi Connected
    tk->events.on(EVT_TK_THREAD,"SYSTEM_EVENT_STA_GOT_IP",[](void* ctx, void* arg){
        Mqtt* _this = (Mqtt*)ctx;
        _this->commit();
    },this);

    tk->variableAdd("mqtt/enable",  enable,   "📡  MQTT Enable");
    tk->variableAdd("mqtt/uri",     uri,      "📡  MQTT URI");

    tk->commandAdd("mqttCommit",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        char OUT[128];
        Mqtt*     mqtt = (Mqtt*) c;
        EspToolkit* tk = (EspToolkit*) mqtt->tk;
        if(parCnt>=2){
            snprintf(OUT,128,"Set  mqtt/enable: %s %s","true",tk->EOL);reply(OUT);
            tk->variableSet("mqtt/enable",(char*)"1");
            snprintf(OUT,128,"Set  mqtt/uri: %s %s","true",tk->EOL);reply(OUT);
            tk->variableSet("mqtt/uri",param[1]);
        };
        tk->variableLoad(true);
        mqtt->commit();
        reply((char*)"DONE! ✅ > Type 'mqttStatus' to check status");
        reply(tk->EOL);
    }, this, "📡 [mqtt/uri] | Apply mqtt settings and connect to configured mqtt server",false);

    tk->commandAdd("mqttPublish",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        Mqtt*     mqtt = (Mqtt*) c;
        EspToolkit* tk = (EspToolkit*) mqtt->tk;
        if(parCnt>=3){
            mqtt->publish(param[1],param[2]);
            reply((char*)"📨 sent!");
            reply(tk->EOL);
        };
    },this,"📡 [topic] [message] | publish a message to topic",false);

    tk->commandAdd("mqttStatus",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        char OUT[128];
        Mqtt*     mqtt = (Mqtt*) c;
        EspToolkit* tk = (EspToolkit*) mqtt->tk;
        snprintf(OUT,128,"%-30s : %s %s","Enable",mqtt->enable ? "true" : "false",tk->EOL);reply(OUT);
        snprintf(OUT,128,"%-30s : %s %s","Connected",tk->status[STATUS_BIT_MQTT]  ? "true" : "false",tk->EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s %s","esp_tls_last_esp_err",esp_err_to_name(mqtt->lastError.esp_tls_last_esp_err),tk->EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d %s","esp_tls_stack_err",mqtt->lastError.esp_tls_stack_err,tk->EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d %s","esp_tls_cert_verify_flags",mqtt->lastError.esp_tls_cert_verify_flags,tk->EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d %s","esp_transport_sock_errno",mqtt->lastError.esp_transport_sock_errno,tk->EOL);reply(OUT);
        char* error_type; 
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_NONE) error_type = "MQTT_ERROR_TYPE_NONE";
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) error_type = "MQTT_ERROR_TYPE_TCP_TRANSPORT";
        if(mqtt->lastError.error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) error_type = "MQTT_ERROR_TYPE_CONNECTION_REFUSED";
        snprintf(OUT,LONG,"%-30s : %s %s","error_type",error_type,tk->EOL);reply(OUT);
        char* connect_return_code;
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_ACCEPTED) connect_return_code = "MQTT_CONNECTION_ACCEPTED";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_PROTOCOL) connect_return_code = "MQTT_CONNECTION_REFUSE_PROTOCOL";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_ID_REJECTED) connect_return_code = "MQTT_CONNECTION_REFUSE_ID_REJECTED";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE) connect_return_code = "MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME) connect_return_code = "MQTT_CONNECTION_REFUSE_BAD_USERNAME";
        if(mqtt->lastError.connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) connect_return_code = "MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED";
        snprintf(OUT,LONG,"%-30s : %s %s","connect_return_code",connect_return_code,tk->EOL);reply(OUT);
    }, this, "📡  Shows MQTT connection Status",false);

}

void Mqtt::commit(){   

    //Destroy Old Session
    if(client){
        esp_mqtt_client_disconnect(client);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }

    //Beginn new Session
    if(enable){
        esp_mqtt_client_config_t cfg{};
        cfg.event_handle = &mqtt_event_handler; 
        cfg.user_context = this;  
        cfg.uri          = uri.c_str();
        cfg.client_id    = tk->hostname.c_str();
        client = esp_mqtt_client_init(&cfg);
        esp_mqtt_client_start(client);
    }

}

esp_err_t Mqtt::mqtt_event_handler(esp_mqtt_event_handle_t event){
    //esp_mqtt_client_handle_t client = event->client;
    Mqtt* _this                     = (Mqtt*)event->user_context;
    //int msg_id = event->msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_CONNECTED");
            _this->tk->events.emit("MQTT_EVENT_CONNECTED");
            _this->tk->status[STATUS_BIT_MQTT] = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_DISCONNECTED");
            _this->tk->events.emit("MQTT_EVENT_DISCONNECTED");
            _this->tk->status[STATUS_BIT_MQTT] = false;
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
            char* topic = (char*) malloc(event->topic_len + 1);
            char* data  = (char*) malloc(event->data_len  + 1);
            xthal_memcpy(topic, event->topic, event->topic_len + 1);
            xthal_memcpy(data,  event->data,  event->data_len  + 1);
            topic[event->topic_len] = '\0';
            data[event->data_len]   = '\0';
            _this->tk->events.emit(topic,(void*)data,event->data_len  + 1);
            delete topic;
            delete data;
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_ERROR");
            _this->tk->status[STATUS_BIT_MQTT] = false;
            _this->lastError = *event->error_handle;
            _this->tk->events.emit("MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(EVT_MQTT_PREFIX, "MQTT_EVENT_OTHER id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

int Mqtt::publish(char* topic, char* message, int qos, int retain){
    if(!client) return -1;
    return esp_mqtt_client_publish(client, topic, message, strlen(message), qos, retain);
};

int Mqtt::subscribe(char* topic, int qos){
    if(!client) return -1;
    return esp_mqtt_client_subscribe(client, topic, qos);
};

int Mqtt::unsubscribe(char* topic){
    if(!client) return -1;
    return esp_mqtt_client_unsubscribe(client, topic);
}; 

#endif