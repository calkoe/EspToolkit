#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <PostOffice.h>

class Button{

    private:

        struct button_t{

            gpio_num_t          gpio;
            gpio_pull_mode_t    pullmode;
            unsigned            wait;
            std::string         topic;

            int                 status;
            int64_t             statusTimestamp;
        };

        PostOffice<std::string>* events;
        std::vector<button_t> buttons;
        SemaphoreHandle_t xBinarySemaphore;
        int64_t getMs();

        void lock(bool = false);
        void unlock(bool = false);

    public:

        Button(PostOffice<std::string>*);
        void add(gpio_num_t gpio, gpio_pull_mode_t pullmode, unsigned wait, std::string topic);
        
};