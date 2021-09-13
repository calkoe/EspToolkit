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

#include "../../EspToolkit.h"

#define BUFFER_SIZE 128

/**
 * @brief   Telnet communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Telnet{

    private:

        static Telnet* _this;
        
        std::string     _buffer;
        bool            _marks{false};
        void in(char);
        static void print(const char* text);

    public:

        Telnet();
        int clientSock;
};