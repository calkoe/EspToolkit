#pragma once
#define _GLIBCXX_USE_C99

#include <string>
#include <fstream>
#include <iostream>
#include <sys/unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_spiffs.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "include/PostOffice.h"
#include "include/SyncTimer.h"
#include "tasks/Button/Button.h"
#include "tasks/Uart/Uart.h"

#define FIRMWARE "EspToolkit 1.1.0"

//  Definitions
#define SHORT               128   
#define LONG                256     
#define SERSPEED            115200 
#define STATUSLED           2
#define STATUSLEDON         1
#define BOOTBUTTON          0
#define EEPROM_SIZE         1024  

// Events
#define EVT_TK_BROADCAST    "tk:broadcast"
#define EVT_TK_COMMAND      "tk:command"

// Status
#define STATUS_BIT_DEFAULT  0
#define STATUS_BIT_SYSTEM   1
#define STATUS_BIT_NETWORK  2
#define STATUS_BIT_MQTT     3
#define STATUS_BIT_APP1     4

/**
 * @brief   EspToolkit
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    4.2021
*/
class EspToolkit{
    
    private:

        //Global
        enum AOS_DT    { AOS_DT_BOOL, AOS_DT_INT, AOS_DT_DOUBLE, AOS_DT_STRING };
        struct AOS_CMD {
            const char* name;
            void        (*function)(void*,void (*)(const char*), char**, uint8_t);
            void*       ctx;
            const char* description;
            bool        hidden;
            AOS_CMD*    aos_cmd;
        };
        struct AOS_VAR {
            const char* name;
            void*       value;
            const char* description;
            bool        hidden;
            bool        protect;
            AOS_DT      aos_dt;
            AOS_VAR*    aos_var;
        };

        static bool             isBegin;
        static char             OUT[LONG];
        static AOS_CMD*         aos_cmd;
        static AOS_VAR*         aos_var;
        static bool             _variableAdd(const char*,void*,const char*,bool,bool,AOS_DT);
        static void             variablesAddDefault();
        static void             commandAddDefault();

    public:   

        //Global
        EspToolkit();
        static void                         begin();
        static void                         loop();
        static PostOffice<std::string>      events;
        static Uart                         uart;
        static SyncTimer                    timer;
        static Button                       button;
        static void                         broadcast(const char* msg);

        //API Settings
        static bool             status[];
        static int              cpuFreq;
        static int              logLevel;
        static int              watchdog;
        static std::string      date;
        static std::string      hostname;
        static std::string      password;
        static bool             locked;
        static std::string      firmware;

        //API Commands
        static bool             commandAdd(const char*,void (*)(void* ctx,void (*reply)(const char*),char** arg, uint8_t argLen),void* ctx,const char* = "",bool = false);
        static void             commandList(const char*,void (*reply)(const char*));
        static void             commandMan(const char*, void (*reply)(const char*));
        static bool             commandCall(const char*,void (*reply)(const char*),char** = 0, uint8_t = 0);
        static void             commandParseAndCall(const char* ca,void (*reply)(const char*));

        //API Variables
        static bool             variableAdd(const char*,bool&,  const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,int&,   const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,double&,const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,std::string&,const char* = "",bool = false,bool = false);
        static void             variableList(const char*,void (*reply)(const char*));
        static bool             variableSet(const char*,char*);
        static void*            variableGet(const char*);
        static void             variableLoad(bool save = false, bool reset = false);
};

// Tools
double                          mapVal(double, int, int, int, int);
inline bool                     sign(double i){return i < 0;};
inline double                   min(double a, double b){return (a < b ? a : b);};
inline double                   max(double a, double b){return (a > b ? a : b);};
std::vector<std::string>        split(std::string s, std::string delimiter);
char*                           loadFile(const char* filename);