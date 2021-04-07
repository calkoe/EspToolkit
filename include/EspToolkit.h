#pragma once
#if defined ESP32

//
//  ESPToolkit.h
//  V0.1
//
//  Created by Calvin Köcher on 15.06.20.
//  Copyright © 2020 Calvin Köcher. All rights reserved.
//  https://github.com/calkoe/ESPToolkit
//

#include <EEPROM.h>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include <PostOffice.h>
#include <Shell.h>
#include <SyncTimer.h>
#include <Button.h>

//  Definitions
#define SHORT               128     //Programm Parameter, Parameter Count
#define LONG                128     //BufferIn, BufferOut, TerminalHistory
#define SERSPEED            115200 
#define STATUSLED           2
#define STATUSLEDON         1
#define BOOTBUTTON          0
#define EEPROM_SIZE         1024  

// Datatypes
#define u8  unsigned char
#define s8  char
#define u16 unsigned int
#define s16 int
#define u32 unsigned long
#define s32 signed long
#define u64 unsigned long long
#define s64 signed long long

// Events
#define EVT_TK_THREAD       0
#define EVT_TK_PREFIX       "tk:"
#define EVT_TK_BROADCAST    "tk:broadcast"

// Status
#define STATUS_BIT_DEFAULT  0
#define STATUS_BIT_SYSTEM   1
#define STATUS_BIT_NETWORK  2
#define STATUS_BIT_MQTT     3
#define STATUS_BIT_APP1     4

//Text
#define textWelcome              "ArduinOS - https://github.com/calkoe/arduinoOS"
#define textCommandNotFound      "Command not found! Try 'help' for more information."
#define textInvalidParameter     "Invalid parameter!"
#define textEnterPassword        "Enter password to login: "
#define textCommands             "Commands:"
#define textVariables            "Variables:"
#define textNotFound             "Parameter not found!"
#define textTasks                "Current Tasks"
#define textEscClear             "\033[2J\033[11H"
#define textOk                   "ok"

class EspToolkit{
    
    private:

        //Global
        enum AOS_DT    { AOS_DT_BOOL, AOS_DT_INT, AOS_DT_DOUBLE, AOS_DT_STRING };
        struct AOS_CMD {
            const char* name;
            void        (*function)(void*,void (*)(char*), char**, u8);
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
        static unsigned         usedEeprom;
        static AOS_CMD*         aos_cmd;
        static AOS_VAR*         aos_var;
        static bool             _variableAdd(const char*,void*,const char*,bool,bool,AOS_DT);
        static void             variablesAddDefault();
        static void             commandAddDefault();

        static SemaphoreHandle_t xBinarySemaphore;
        static void lock(bool = false);
        static void unlock(bool = false);

    public:   

        //Global
        EspToolkit();
        static void                         begin();
        static void                         loop();
        static PostOffice<std::string>      events;
        static Shell<128>                   shell;
        static SyncTimer                    timer;
        static Button                       button;
        static char*                        EOL;
        static void                         broadcast(char* msg);

        //API Settings
        static bool             status[];
        static u16              watchdog;
        static bool             autoReset;
        static bool             locked;
        static String           date;
        static String           date_temp;
        static String           hostname;
        static String           password;
        static String           firmware;

        //API Commands
        static bool             commandAdd(const char*,void (*)(void* ctx,void (*reply)(char*),char** arg, u8 argLen),void* ctx,const char* = "",bool = false);
        static void             commandList(const char*,void (*reply)(char*));
        static void             commandMan(const char*, void (*reply)(char*));
        static bool             commandCall(const char*,void (*reply)(char*),char** = 0, u8 = 0);
        static void             commandParseAndCall(char* ca,void (*reply)(char*));

        //API Variables
        static bool             variableAdd(const char*,bool&,  const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,int&,   const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,double&,const char* = "",bool = false,bool = false);
        static bool             variableAdd(const char*,String&,const char* = "",bool = false,bool = false);
        static void             variableList(const char*,void (*reply)(char*));
        static bool             variableSet(const char*,char*);
        static void*            variableGet(const char*);
        static void             variableLoad(bool = false);

        // Tools
        inline double           mapVal(double, int, int, int, int);
        inline bool             sign(double i){return i < 0;};

};
#endif