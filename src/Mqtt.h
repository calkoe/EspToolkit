#pragma once

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

        static Mqtt* _this;
        EspToolkit* tk;
        esp_mqtt_client_handle_t client{nullptr};
        std::map<std::string,int> subscriptions;
        static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(const char* str);
        };
        std::string _buffer;
        bool _marks{false};
        void in(char);

    public:

        //Global
        Mqtt(EspToolkit*);
        void commit();
        void publish(std::string topic, const char* message, int qos = 0, int retain = 0);
        void subscribe(std::string topic, int qos = 0);
        void unsubscribe(std::string topic); 
        esp_mqtt_error_codes_t lastError{};
        static void print(const char* text);

        //LOW LEVEL CONFIG
        esp_mqtt_client_config_t    config{};

        //CONFIG
        bool                        enable{false};
        std::string                 commandTopic;
        std::string                 clientCert;
        std::string                 clientKey;
        std::string                 caCert;
        bool                        caVerify{false};
        std::string                 uri{"mqtt://test.mosquitto.org:1883"};

};