#pragma once

#include <iostream>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
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
        static void uart_event_task(void *pvParameters);

    public:

        Uart();

};

#include "../../EspToolkit.h"