#pragma once

#include <stdio.h>
#include <stdlib.h>
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
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "include/PostOffice.h"
#include "include/SyncTimer.h"
#include "tasks/Button/Button.h"
#include "tasks/Uart/Uart.h"

#define TOOLKITVERSION "EspToolkit 1.3.0"

// FEATURES
// #define TK_DISABLE_GPIO_CONTROL
// #define TK_DISABLE_UART

//  Definitions
#define LONG                256     
#define SERSPEED            115200 
#define BOOTBUTTON_PIN      0

// Status
#define STATUS_BIT_DEFAULT  0
#define STATUS_BIT_SYSTEM   1
#define STATUS_BIT_NETWORK  2
#define STATUS_BIT_MQTT     3
#define STATUS_BIT_APP1     4
#define STATUS_LED_PIN      2
#define STATUS_LED_ON       1

/**
 * @brief   EspToolkit
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    7.2021
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

        bool             isBegin{false};
        AOS_CMD*         aos_cmd{nullptr};
        AOS_VAR*         aos_var{nullptr};
        bool             _variableAdd(const char*,void*,const char*,bool,bool,AOS_DT);
        void             variablesAddDefault();
        void             commandAddDefault();

    public:   

        //TOOLS
        EspToolkit();
        void                        begin();
        void                        loop();
        PostOffice<std::string>     events{100};
        Button                      button{&events};
        SyncTimer                   timer;
        Uart*                       uart{nullptr};

        //CONFIG
        bool                        status[5]{true};
        int                         cpuFreq{2};
        int                         logLevel{0};
        int                         watchdog{60};
        std::string                 hostname{"EspToolkit"};;
        std::string                 password{"tk"};
        bool                        locked{false};
        std::string                 appVersion{"generic"};

        //API Commands
        bool                        commandAdd(const char*,void (*)(void* ctx,void (*reply)(const char*),char** arg, uint8_t argLen),void* ctx,const char* = "",bool = false);
        void                        commandList(const char*,void (*reply)(const char*));
        void                        commandMan(const char*, void (*reply)(const char*));
        bool                        commandCall(const char*,void (*reply)(const char*),char** = 0, uint8_t = 0);
        void                        commandParseAndCall(char* ca,void (*reply)(const char*));

        //API Variables
        bool                        variableAdd(const char*,bool&,  const char* = "",bool = false,bool = false);
        bool                        variableAdd(const char*,int&,   const char* = "",bool = false,bool = false);
        bool                        variableAdd(const char*,double&,const char* = "",bool = false,bool = false);
        bool                        variableAdd(const char*,std::string&,const char* = "",bool = false,bool = false);
        void                        variableLoad(bool save = false, bool reset = false);
};

// Globals
static EspToolkit*                  EspToolkitInstance{nullptr};
static char                         OUT[LONG]{0};


static double                       mapVal(double, int, int, int, int);
static inline bool                  sign(double i){return i < 0;};
static inline double                min(double a, double b){return (a < b ? a : b);};
static inline double                max(double a, double b){return (a > b ? a : b);};
static std::vector<std::string>     split(std::string s, std::string delimiter);
static char*                        loadFile(const char* filename);