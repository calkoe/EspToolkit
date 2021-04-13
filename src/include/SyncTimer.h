#pragma once
#if defined ESP32

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"

/**
 * @brief Simple synchronous timer library for setting intervalls and timeouts
 * @author Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date 4.2021
*/
class SyncTimer{

        private:

            struct AOS_TASK {
                uint16_t                 id;
                void                (*function)(void*);
                void*               ctx;              
                int64_t             timestamp;
                uint32_t            interval;
                const char*         description;  
                uint16_t            time;  
                uint16_t            timeMax;  
                bool                repeat;
                AOS_TASK*           aos_task;
            };

            AOS_TASK* aos_task{nullptr};
            SemaphoreHandle_t xBinarySemaphore;
            void lock(bool = false);
            void unlock(bool = false);

        public:

            const char*            EOL{"\r\n"};

            SyncTimer();
            uint16_t         setInterval(void(*)(void* ctx),void* ctx,uint16_t intervall,const char* name = "");
            uint16_t         setTimeout(void(*)(void* ctx),void* ctx,uint16_t timeout,const char* name = "",bool = false);
            AOS_TASK*        unsetInterval(uint16_t id);
            void             loop();
            void             printTasks(void (*reply)(char*));

};


inline SyncTimer::SyncTimer(){
    xBinarySemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xBinarySemaphore);  
}

/**
 * Set Interval
 * 
 * @param  function Function Callback
 * @param  intervall
 * @return intervall Id
*/
inline uint16_t SyncTimer::setInterval(void(*f)(void*),void* a,uint16_t i,const char* description){
    return setTimeout(f,a,i,description,true);
};

/**
 * Set Timeout
 * 
 * @param  function Function Callback
 * @param  intervall 
 * @param  repeat 
 * @return timeout Id
*/
inline uint16_t SyncTimer::setTimeout(void(*f)(void*),void* ctx,uint16_t i, const char* description, bool repeat){
    if(i <= 0) return 0;
    lock();
    static uint16_t idSerial{1};
    AOS_TASK* e = new AOS_TASK{idSerial,f,ctx,esp_timer_get_time(),(i*(uint32_t)1000),description,0,0,repeat,nullptr};
    if(!aos_task){
        aos_task = e;
    }else{
        AOS_TASK* i{aos_task};
        while(i->aos_task) i = i->aos_task;
        i->aos_task = e;
    }
    unlock();
    return idSerial++;
};

/**
 * Unset Interval
 * 
 * @param idDel IntervalID
*/
inline SyncTimer::AOS_TASK* SyncTimer::unsetInterval(uint16_t idDel){
    lock();
    if(!aos_task) return nullptr;
    if(aos_task->id == idDel){
        AOS_TASK* d{aos_task}; 
        aos_task = aos_task->aos_task;
        delete d;
        return nullptr;
    }
    AOS_TASK* i{aos_task};
    while(i->aos_task){
        if(i->aos_task->id == idDel){
            AOS_TASK* d{i->aos_task}; 
            i->aos_task = i->aos_task->aos_task;
            delete d;
            return i->aos_task;
        }
        i = i->aos_task;
    }
    unlock();
    return nullptr;
};

/**
 * Manage Task Loop
*/
inline void SyncTimer::loop(){
    lock();
    AOS_TASK* i{aos_task};
    while(i){
        int64_t t1 = esp_timer_get_time();
        if(!i->interval || (int64_t)(t1-i->timestamp)>=i->interval){
            //reply("Abweichung: " + (String)(unsigned long)(ms-i->timestamp));
            unlock();
                (*i->function)(i->ctx); 
                taskYIELD();
            lock();
            int64_t t2 = esp_timer_get_time();
            i->time = (int64_t)(t2 - t1);
            if(i->time>i->timeMax) i->timeMax = i->time; 
            i->timestamp = t2;
            i = !i->repeat ? unsetInterval(i->id) : i->aos_task;
        }else i = i->aos_task;
    }   
    unlock();
};

/**
 * Display Current Tasks
*/
inline void SyncTimer::printTasks(void (*reply)(char*)){
    reply((char*)"Tasks: \r\n");
    reply((char*)EOL);
    AOS_TASK* i{aos_task};
    while(i){
        char OUT[128];
        snprintf(OUT,128,"> %-20s : i: %-9i t#: %-9i t+: %-5i %s",i->description,i->interval/1000,i->time/1000,i->timeMax/1000,EOL);
        reply(OUT);
        i = i->aos_task;
    };
};

/**
 * Thread Safe
*/
inline void SyncTimer::lock(bool isr){
    if(isr)
        xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    else
        xSemaphoreTakeFromISR(xBinarySemaphore,0);
}

inline void SyncTimer::unlock(bool isr){
    if(isr)
        xSemaphoreGive(xBinarySemaphore);
    else
        xSemaphoreGiveFromISR(xBinarySemaphore,0);  
}

#endif