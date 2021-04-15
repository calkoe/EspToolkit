#if defined ESP32

#include "Uart.h"


Uart::Uart(PostOffice<std::string>* events, const char* commandTopic, const char* broadcastTopic):events{events},commandTopic{commandTopic}{

    lineIn.setOnEcho([](char* str, void* ctx){
        std::cout << str;
    },NULL);

    lineIn.setOnLine([](char* str, void* ctx){
        Uart* _this = (Uart*) ctx;
        simple_cmd_t simple_cmd{
            strdup(str), 
            print
        };
        _this->events->emit(_this->commandTopic,(void*)&simple_cmd,sizeof(simple_cmd_t));
    },this);

    // EVT_TK_BROADCAST -> SERIAL
    events->on(0,broadcastTopic,[](void* ctx, void* arg){
        std::cout << (char*)arg;
    },NULL);

    // TASK
    xTaskCreate([](void* ctx){
        Uart* _this = (Uart*) ctx;
        uint8_t c;

        // Say Hello!
        simple_cmd_t simple_cmd{
            strdup("help"), 
            print
        };
        _this->events->emit(_this->commandTopic,(void*)&simple_cmd,sizeof(simple_cmd_t));

        while(true){
            while((c = (uint8_t)fgetc(stdin)) != 0xFF) _this->lineIn.in(c);
            vTaskDelay(10);
        }
    }, "uart", 2048, this, 0, NULL);
    
};

void Uart::print(const char* text){
    std::cout << text;
};


#endif