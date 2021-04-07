#pragma once
#if defined ESP32

#include "mqtt_client.h"
#include "EspToolkit.h"
 
#define EVT_MQTT_PREFIX "mqtt:"

class Mqtt{

    private:

        EspToolkit* tk;
        esp_mqtt_client_handle_t client{NULL};
        static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t);

    public:

        //Global
        Mqtt(EspToolkit*);
        void commit();
        int  publish(char* topic, char* message, int qos = 0, int retain = 0);
        int  subscribe(char* topic, int qos = 0);
        int  unsubscribe(char* topic); 
        esp_mqtt_error_codes_t lastError{};

        //CONFIG
        bool          enable{true};
        String        uri{"mqtt://test.mosquitto.org:1883"};

};


#endif