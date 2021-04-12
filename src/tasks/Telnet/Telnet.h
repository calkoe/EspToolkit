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

#include "../../include/LineIn.h"
#include "../../include/PostOffice.h"

#define TAG "telnet"
#define EVT_TELNET_THREAD 2

/**
 * @brief   Telnet communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Telnet{

    private:
        PostOffice<std::string>* events;
        LineIn<128> lineIn;
        char* commandTopic;
        char* broadcastTopic;
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(char* str);
        };


    public:

        Telnet(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic);
        
        void    print(char* text);
        void    lock();
        void    unlock();
        void    setPassword(char* password);

};