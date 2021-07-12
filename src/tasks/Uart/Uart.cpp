#include "Uart.h"

Uart* Uart::_this{nullptr};

Uart::Uart(){
    if(_this) return;
    _this = this;

    lineIn.setOnEcho([](char* str, void* ctx){
        std::cout << str;
    },NULL);

    lineIn.setOnLine([](char* str, void* ctx){
        //EspToolkitInstance->commandParseAndCall(str,print);
    },NULL);

    // TASK
    xTaskCreate([](void* ctx){
        uint8_t c;

        // Say Hello!
        // EspToolkitInstance->commandParseAndCall((char*)"help",print);

        while(true){
            //while((c = (uint8_t)fgetc(stdin)) != 0xFF) _this->lineIn.in(c);
            while(true)
                continue;
            vTaskDelay(10);
        }

    }, "uart", 2048, NULL, 0, NULL);
    
};

void Uart::print(const char* text){
    std::cout << text;
};