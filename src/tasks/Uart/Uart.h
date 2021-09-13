#pragma once

#include <iostream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"

#include "../../include/LineIn.h"

/**
 * @brief   Uart communication
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Uart{

    private:

        static Uart* _this;
        LineIn      lineIn;
       
        static void print(const char* text);

    public:

        Uart();

};

#include "../../EspToolkit.h"