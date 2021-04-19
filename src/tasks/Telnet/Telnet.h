#pragma once

#include <iostream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <lwip/sockets.h>
#include <esp_log.h>
#include <string.h>
#include <errno.h>
#include "sdkconfig.h"

#include "../../include/PostOffice.h"

#define TAG "telnet"
#define BUFFER_SIZE 128

/**
 * @brief   Telnet communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Telnet{

    private:

        static Telnet* _this;
        PostOffice<std::string>* events;
        char* commandTopic;
        char* broadcastTopic;
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(const char* str);
        };



    public:

        Telnet(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic);
        int clientSock;
        static void print(const char* text);
};