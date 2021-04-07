#if defined ESP32

#include <Button.h>

Button::Button(PostOffice<std::string>* events):events{events}{

    xBinarySemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xBinarySemaphore);  
    
    xTaskCreate([](void* arg){
        Button* _this = (Button*) arg;
        while(true){
            _this->lock();
            for(uint8_t i = 0; i != _this->buttons.size(); i++){
                int status = gpio_get_level(_this->buttons[i].gpio);
                if(status == _this->buttons[i].status)
                    _this->buttons[i].statusTimestamp = _this->getMs();
                else if((int64_t)(_this->getMs() - _this->buttons[i].statusTimestamp) > _this->buttons[i].wait){
                    _this->buttons[i].status = status;
                    _this->events->emit(_this->buttons[i].topic,(void*)&status,sizeof(int));
                }
            }
            _this->unlock();
            vTaskDelay(10);
        }
    }, "button", 2048, this, 1, NULL);
    
}

void Button::add(gpio_num_t gpio, gpio_pull_mode_t pullmode, unsigned wait, std::string topic){
    lock();

    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio, pullmode);

    buttons.insert(buttons.end(),(button_t){
        .gpio               = gpio,
        .pullmode           = pullmode,
        .wait               = wait,
        .topic              = topic,
        .status             = gpio_get_level(gpio),
        .statusTimestamp    = getMs(),
    });

    unlock();
};

int64_t Button::getMs(){
    return esp_timer_get_time()/1000;
}

void Button::lock(bool isr){
    if(isr)
        xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    else
        xSemaphoreTakeFromISR(xBinarySemaphore,0);
}

void Button::unlock(bool isr){
    if(isr)
        xSemaphoreGive(xBinarySemaphore);
    else
        xSemaphoreGiveFromISR(xBinarySemaphore,0);  
}

#endif