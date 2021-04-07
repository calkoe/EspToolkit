#pragma once
#if defined ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"

#define u8  unsigned char
#define s8  char
#define u16 unsigned int
#define s16 int
#define u32 unsigned long
#define s32 signed long
#define u64 unsigned long long
#define s64 signed long long

class SyncTimer{

        private:

            struct AOS_TASK {
                u16                 id;
                void                (*function)(void*);
                void*               ctx;              
                u64                 timestamp;
                u16                 interval;
                const char*         description;  
                u16                 time;  
                u16                 timeMax;  
                bool                repeat;
                AOS_TASK*           aos_task;
            };

            AOS_TASK* aos_task{nullptr};
            SemaphoreHandle_t xBinarySemaphore;
            void lock(bool = false);
            void unlock(bool = false);

        public:

            char*            EOL{(char*)"\r\n"};

            SyncTimer();
            u16              setInterval(void(*)(void* ctx),void* ctx,u16 intervall,const char* name = "");
            u16              setTimeout(void(*)(void* ctx),void* ctx,u16 timeout,const char* name = "",bool = false);
            AOS_TASK*        unsetInterval(u16 id);
            void             loop();
            void             printTasks(void (*reply)(char*));

};

#endif