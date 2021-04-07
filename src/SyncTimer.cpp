#if defined ESP32

#include <SyncTimer.h>

SyncTimer::SyncTimer(){
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
u16 SyncTimer::setInterval(void(*f)(void*),void* a,u16 i,const char* description){
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
u16 SyncTimer::setTimeout(void(*f)(void*),void* ctx,u16 i, const char* description, bool repeat){
    lock();
    static u16 idSerial{1};
    AOS_TASK* e = new AOS_TASK{idSerial,f,ctx,(u64)esp_timer_get_time()/1000,i,description,0,0,repeat,nullptr};
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
SyncTimer::AOS_TASK* SyncTimer::unsetInterval(u16 idDel){
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
void SyncTimer::loop(){
    lock();
    AOS_TASK* i{aos_task};
    u64 t1{0};
    u64 t2{0};
    while(i){
        t1 = (u64)(esp_timer_get_time()/1000);
        if(!i->interval || (u64)(t1-i->timestamp)>=i->interval){
            //reply("Abweichung: " + (String)(unsigned long)(ms-i->timestamp));
            unlock();
                (*i->function)(i->ctx); 
                taskYIELD();
            lock();
            t2 = (u64)(esp_timer_get_time()/1000);
            i->time = (u64)(t1 - t1);
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
void SyncTimer::printTasks(void (*reply)(char*)){
    reply((char*)"Tasks: \r\n");
    reply(EOL);
    AOS_TASK* i{aos_task};
    while(i){
        char OUT[128];
        snprintf(OUT,128,"> %-20s : i: %-9i t#: %-9i t+: %-5i %s",i->description,i->interval,i->time,i->timeMax,EOL);
        reply(OUT);
        i = i->aos_task;
    };
};

void SyncTimer::lock(bool isr){
    if(isr)
        xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    else
        xSemaphoreTakeFromISR(xBinarySemaphore,0);
}

void SyncTimer::unlock(bool isr){
    if(isr)
        xSemaphoreGive(xBinarySemaphore);
    else
        xSemaphoreGiveFromISR(xBinarySemaphore,0);  
}

#endif