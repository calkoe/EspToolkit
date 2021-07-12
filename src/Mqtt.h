#pragma once

#include <map>
#include <string>
#include "mqtt_client.h"
#include "EspToolkit.h"
 
/**
 * @brief   Mqtt
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Mqtt{

    private:

        static Mqtt* _this;
        std::map<std::string,int> subscriptions;
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
       
        std::string _buffer;
        bool _marks{false};
        void in(char);
        static void print(const char* text);

    public:

        //Global
        Mqtt();
        void commit();
        void publish(std::string topic, const char* message, int qos = 0, int retain = 0);
        void subscribe(std::string topic, int qos = 0);
        void unsubscribe(std::string topic); 
        esp_mqtt_client_handle_t client{nullptr};
        esp_mqtt_error_codes_t   lastError{};

        //LOW LEVEL CONFIG
        esp_mqtt_client_config_t    config{};

        //CONFIG
        bool                        enable{false};
        std::string                 uri{"mqtt://test.mosquitto.org:1883"};
        std::string                 clientCert;
        std::string                 clientKey;
        std::string                 caCert;
        bool                        caVerify{false};
        std::string                 commandTopic;

};