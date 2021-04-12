#pragma once

#include <iostream>
#include <stdio.h>
#include <string.h> 
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "../../include/PostOffice.h"

/**
 * @brief   Thread-Safe an non-blocking GPIO observer library
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class Button{

    private:

        QueueHandle_t add_queue;
        struct add_t{
            gpio_num_t          gpio;
            gpio_pull_mode_t    pullmode;
            unsigned            wait;
            char*               topic;
        };

        QueueHandle_t remove_queue;
        PostOffice<std::string>* events;
        int         taskStack;
        uint8_t     taskPrio;
        TickType_t  taskDelay;

    public:

        /**
         * @param events The PostOffice event Library is used for emitting GPIO events
         * @param taskStack RTOS task stack size
         * @param taskPrio  RTOS task PRIO
         * @param taskDelay for each loop in Ticks
        */
        Button(PostOffice<std::string>* events, int taskStack = 4096, uint8_t taskPrio = 10, TickType_t taskDelay = 10);

        /**
         * @brief Start observing a GPIO
         * @param gpio GPIO to observe (same GPIO can be added multiple times with different topic/wait-times)
         * @param pullmode Pullmode of GPIO
         * @param wait time to wait on new status before triggering in ms. Can also be used to detect 'long press' on button (10-20ms for debouncing)
         * @param topic Events will send to this topic via PostOffice. Get GPIO status will be send as int pointer, access via: int status = *(int*)arg;
        */
        void add(gpio_num_t gpio, gpio_pull_mode_t pullmode, unsigned wait, char* topic);

        /**
         * @brief Stop observing GPIO
         * @param gpio GPIO
        */
        void remove(gpio_num_t gpio);
        
};