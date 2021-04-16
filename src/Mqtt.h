#pragma once
#if defined ESP32

#include <map>
#include <string>
#include "mqtt_client.h"
#include "EspToolkit.h"
 
#define EVT_MQTT_PREFIX "mqtt:"

/**
 * @brief   Mqtt
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Mqtt{

    private:

        EspToolkit* tk;
        esp_mqtt_client_handle_t client{NULL};
        std::map<std::string,int> subscriptions;
        static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t);

    public:

        //Global
        Mqtt(EspToolkit*);
        void commit();
        void publish(std::string topic, const char* message, int qos = 0, int retain = 0);
        void subscribe(std::string topic, int qos = 0);
        void unsubscribe(std::string topic); 
        esp_mqtt_error_codes_t lastError{};

        //LOW LEVEL CONFIG
        esp_mqtt_client_config_t    config{};

        //CONFIG
        bool                        enable{true};
        std::string                 uri{"mqtt://test.mosquitto.org:1883"};

};


#endif