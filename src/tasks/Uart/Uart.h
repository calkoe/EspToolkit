#pragma once

#include <iostream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "../../include/LineIn.h"
#include "../../include/PostOffice.h"

#define TAG "uart"

/**
 * @brief   Uart communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Uart{

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

        Uart(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic);
        static void print(char* text);

};