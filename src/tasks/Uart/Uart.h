#pragma once

#include <iostream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "../../include/LineIn.h"
#include "../../include/PostOffice.h"

/**
 * @brief   Uart communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Uart{

    private:

        static Uart* _this;
        PostOffice<std::string>* events;
        LineIn      lineIn;
        const char* commandTopic;
        const char* broadcastTopic;
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(const char* str);
        };

    public:

        Uart(PostOffice<std::string>* events, const char* commandTopic, const char* broadcastTopic);
        static void print(const char* text);

};