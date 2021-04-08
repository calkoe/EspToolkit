#if defined ESP32

#include "Uart.h"


Uart::Uart(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic):events{events},commandTopic{commandTopic}{

    lineIn.setOnEcho([](char* str, void* ctx){
        std::cout << str;
    },this);

    lineIn.setOnLine([](char* str, void* ctx){
        Uart* _this = (Uart*) ctx;
        simple_cmd_t simple_cmd;
        simple_cmd.payload = str;
        simple_cmd.reply   = [](char* str){std::cout << str;};
        _this->events->emit(_this->commandTopic,(void*)&simple_cmd,sizeof(simple_cmd_t));
    },this);

    // EVT_TK_BROADCAST -> SERIAL
    events->on(EVT_UART_THREAD,broadcastTopic,[](void* ctx, void* arg){
        std::cout << (char*)arg;
    },NULL);

     xTaskCreate([](void* ctx){
        Uart* _this = (Uart*) ctx;
        uint8_t c;
        while(true){
            while((c = (uint8_t)fgetc(stdin)) != 0xFF) _this->lineIn.in(c);
            _this->events->loop(EVT_UART_THREAD);
            vTaskDelay(10);
        }
    }, "Uart", 2048, this, 1, NULL);
    
};

void Uart::lock(){
    this->lineIn.lock();
};

void Uart::unlock(){
    this->lineIn.unlock();

};

void Uart::setPassword(char* password){
    this->lineIn.setPassword(password);
};

#endif