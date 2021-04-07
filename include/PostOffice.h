
#pragma once
#if defined ESP32

#include <vector>
#include <map>
#include <functional> 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

template <class T>
class PostOffice{

    private:

        uint16_t queueLength;

        struct callback_t{
            uint8_t thread;
            void    (*callback)(void*,void*);
            void*   ctx;
            void*   arg;
            size_t  argSize;
        };

        SemaphoreHandle_t xBinarySemaphore;
        std::map<T,std::vector<callback_t>> callbacks;
        std::map<uint8_t,QueueHandle_t>     tasks;
        void lock(bool = false);
        void unlock(bool = false);

    public:

        PostOffice(uint16_t);
        ~PostOffice();

        void        on(uint8_t thread,T topic,void(*callback)(void* ctx,void* arg),void* ctx = nullptr ,bool isr = false);
        bool        emit(T topic,void* arg = nullptr,size_t argSize = 0,bool isr = false);
        uint16_t    loop(uint8_t thread, bool isr = false);

};

// Constructor
template <typename T>
PostOffice<T>::PostOffice(uint16_t queueLength) : queueLength{queueLength}{
    xBinarySemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xBinarySemaphore);
    callbacks = std::map<T,std::vector<callback_t>>();
    tasks     = std::map<uint8_t,QueueHandle_t>();
}

// Destructor 
template <typename T>
PostOffice<T>::~PostOffice(){

}

// Adds callback_t to callbacks
template <typename T>
void PostOffice<T>::on(uint8_t thread,T topic,void (*callback)(void* ctx,void* arg), void* ctx, bool isr){

    lock(isr);

    //  Create Topic Vector if needed
    if(callbacks.find(topic) == callbacks.end())
        callbacks.insert( std::pair< T,std::vector<callback_t> >( topic, std::vector<callback_t>()));

    //  Insert Callback in Topic Vector
    callbacks[topic].insert(callbacks[topic].end(),(callback_t){
        .thread     = thread,
        .callback   = callback,
        .ctx        = ctx,
    });

    unlock(isr);

};

// Add callback_t to tasks
template <typename T>
bool PostOffice<T>::emit(T topic, void* arg, size_t argSize, bool isr){

    lock(isr);

    //  Check if there any Callbacks registert for topic
    if(callbacks.find(topic) == callbacks.end()){
        
        /*
        std::cout << "Topic: " << topic << " not found!" << std::endl;
            for(int i=0; i<strlen(topic) + 1; i++)
                        std::cout << std::hex << (int)topic[i] << std::endl;

        std::cout << "Aviable: " << std::endl;
        for(auto elem : callbacks){
            std::cout << elem.first << "\n";
            for(int i=0; i<strlen(elem.first) + 1; i++)
                std::cout << std::hex << (int)elem.first[i] << std::endl;
        }
        */

        unlock(isr);
        return false;
    }

    //  For all Callbacks of Topic
    bool ret{true};
    for(callback_t& callback: callbacks[topic]) {

        //  Create Thread Queue if needed
        if(tasks.find(callback.thread) == tasks.end())
            tasks.insert(std::pair<uint8_t,QueueHandle_t>( callback.thread, xQueueCreate( this->queueLength, sizeof(callback_t))));

        //  Save arg to Heap and add task_t to thread specific queue
        if(arg){
            callback.argSize   = argSize;
            callback.arg       = malloc(callback.argSize);
            xthal_memcpy(callback.arg, arg, callback.argSize);
        }
        if(xQueueSend(tasks[callback.thread], (void*) &callback, 0) == pdFALSE) ret = false;
        
    }

    unlock(isr);
    return ret;

};

// Recieves callback_t from tasks
template <typename T>
uint16_t PostOffice<T>::loop(uint8_t thread, bool isr){

    //  Check if Thread Queue exitst
    QueueHandle_t queue; 
    if(tasks.find(thread) == tasks.end()){
        return 0;
    }else{
        queue = tasks[thread];  
    }
    
    // Recieves Callbacks from Thread Queue
    uint16_t   n;
    callback_t task;
    while( xQueueReceive( queue ,&(task),0) ){
        (*task.callback)(task.ctx,task.arg);    
        if(task.arg) free(task.arg);
        n++;
    }
    return n;

};

template <typename T>
void PostOffice<T>::lock(bool isr){
    if(isr)
        xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    else
        xSemaphoreTakeFromISR(xBinarySemaphore,0);
}

template <typename T>
void PostOffice<T>::unlock(bool isr){
    if(isr)
        xSemaphoreGive(xBinarySemaphore);
    else
        xSemaphoreGiveFromISR(xBinarySemaphore,0);  
}

#endif