#if defined ESP32

#include "Button.h"

Button::Button(PostOffice<std::string>* events, int taskStack, uint8_t taskPrio, TickType_t taskDelay):events{events},taskStack{taskStack},taskPrio{taskPrio},taskDelay{taskDelay}{

    add_queue       = xQueueCreate(5, sizeof(add_t));
    remove_queue    = xQueueCreate(5, sizeof(gpio_num_t));

    xTaskCreate([](void* arg){
        Button* _this = (Button*) arg;

        // SETUP
        struct button_t{
            gpio_num_t          gpio;
            gpio_pull_mode_t    pullmode;
            unsigned            wait;
            const char*         topic;
            int                 status;
            int64_t             statusTimestamp;
        };
        std::vector<button_t> buttons;

        // LOOP
        while(true){

            // void add()
            add_t add;
            while(xQueueReceive(_this->add_queue,&add,0) == pdTRUE){
                gpio_reset_pin(add.gpio);
                gpio_set_direction(add.gpio, GPIO_MODE_INPUT);
                gpio_set_pull_mode(add.gpio, add.pullmode);
                buttons.insert(buttons.end(),(button_t){
                    .gpio               = add.gpio,
                    .pullmode           = add.pullmode,
                    .wait               = add.wait * 1000,
                    .topic              = add.topic,
                    .status             = gpio_get_level(add.gpio),
                    .statusTimestamp    = esp_timer_get_time(),
                });
            }

            // void remove()
            gpio_num_t remove;
            while(xQueueReceive(_this->remove_queue,&remove,0) == pdTRUE){
                gpio_reset_pin(remove);
                for(auto i = buttons.begin(); i != buttons.end();){
                    if(i->gpio == remove){
                        i = buttons.erase(i);
                    }else{
                        i++;
                    }
                }
            }

            // Work
            for(auto i = buttons.begin(); i != buttons.end(); i++){
                int status = gpio_get_level(i->gpio);
                if(status == i->status)
                    i->statusTimestamp = esp_timer_get_time();
                else if((int64_t)(esp_timer_get_time() - i->statusTimestamp) > i->wait){
                    i->status = status;
                    _this->events->emit(i->topic,(void*)&status,sizeof(int));
                }
            }
            
            vTaskDelay(_this->taskDelay);
        }
    }, "Button", taskStack, this, taskPrio, NULL);
    
}

void Button::add(gpio_num_t gpio, gpio_pull_mode_t pullmode, unsigned wait, const char* topic){
    add_t add{gpio,pullmode,wait,topic};
    xQueueSend(add_queue, (void*)&add, portMAX_DELAY);
};

void Button::remove(gpio_num_t gpio){
    xQueueSend(remove_queue, (void*)&gpio, portMAX_DELAY);
};


#endif