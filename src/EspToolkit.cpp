#if defined ESP32

#include "EspToolkit.h"

//Global
PostOffice<std::string> EspToolkit::events(100);
Shell<128>              EspToolkit::shell;
SyncTimer               EspToolkit::timer;
Button                  EspToolkit::button(&events);
char*                   EspToolkit::EOL{"\r\n"};
bool                    EspToolkit::isBegin{false};
EspToolkit::AOS_CMD*    EspToolkit::aos_cmd{nullptr};
EspToolkit::AOS_VAR*    EspToolkit::aos_var{nullptr};
char                    EspToolkit::OUT[LONG]{0};
bool                    EspToolkit::status[5]{true};
unsigned                EspToolkit::usedEeprom{0};
u16                     EspToolkit::watchdog{0};
bool                    EspToolkit::autoReset{true};
bool                    EspToolkit::locked{false};
String                  EspToolkit::date{__DATE__ " " __TIME__};
String                  EspToolkit::date_temp{date};
String                  EspToolkit::hostname{"EspToolkit"};
String                  EspToolkit::password{"tk"};
String                  EspToolkit::firmware{"-"};
SemaphoreHandle_t       EspToolkit::xBinarySemaphore;

EspToolkit::EspToolkit(){

    status[STATUS_BIT_SYSTEM] = false;
    
    xBinarySemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xBinarySemaphore); 

    // BUTTON RESET
    button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,5000,"bootbutton5000ms");
    events.on(EVT_TK_THREAD,"bootbutton5000ms",[](void* ctx, void* arg){
        EspToolkit* _this = (EspToolkit*) ctx;
        if(!*(bool*)arg){
            ESP_LOGE(EVT_TK_PREFIX, "BUTTON RESET");
            date=date_temp;
            variableLoad(true);
            ESP.restart();
        }
    },this);

    // Status LED
    if(STATUSLED>=0){
        xTaskCreate([](void* arg){
            EspToolkit* _this = (EspToolkit*) arg;
            gpio_reset_pin((gpio_num_t)STATUSLED);
            gpio_set_direction((gpio_num_t)STATUSLED, GPIO_MODE_OUTPUT);
            while(true){
                for(u8 i{0};i<5;i++){
                    if(_this->status[i]) gpio_set_level((gpio_num_t)STATUSLED,STATUSLEDON);
                    vTaskDelay(200);
                    gpio_set_level((gpio_num_t)STATUSLED,!STATUSLEDON);
                    vTaskDelay(200);
                }
                vTaskDelay(600);
            }
        }, "statusled", 2048, this, 1, NULL);
    }
    
    // SHELL
    shell.hostname = (char*)hostname.c_str();
    shell.password = (char*)password.c_str();

    // SHELL -> SERIAL
    shell.setOnEcho([](char* ca, void* arg){
        std::cout << ca;
    },this);

    // SHELL -> COMMAND
    shell.setOnLine([](char* ca, void* arg){
        commandParseAndCall(ca,[](char* ca){std::cout << ca; });
    },this);

    // EVT_TK_BROADCAST -> SERIAL
    events.on(0,EVT_TK_BROADCAST,[](void* ctx, void* arg){
        std::cout << (char*)arg;
    },NULL);

    // Toolkit
    variablesAddDefault();
    commandAddDefault();

}

void EspToolkit::begin(){

    // Initialize Logging
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    ESP_LOGI(EVT_TK_PREFIX, "[APP] Startup..");
    ESP_LOGI(EVT_TK_PREFIX, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(EVT_TK_PREFIX, "[APP] IDF version: %s", esp_get_idf_version());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    variableLoad();

    // Initialize Watchdog
    if(watchdog){
        esp_task_wdt_init(watchdog, true); 
        esp_task_wdt_add(NULL); 
    }

    status[STATUS_BIT_SYSTEM] = true;

}

void EspToolkit::loop(){
    // Serial
    uint8_t c = (uint8_t)fgetc(stdin);
	if (c!=0xFF){
        shell.in(c);
    }
    if(watchdog) esp_task_wdt_reset(); 
    timer.loop();
    events.loop(EVT_TK_THREAD);
};

/**
 *  Emits a Message event to EVT_TK_BROADCAST 
 */
void EspToolkit::broadcast(char* msg){
    events.emit(EVT_TK_BROADCAST,msg,strlen(msg)+1);
};

/**
 * Adds a new Command to CLI
 * 
 * @param name Command Name
 * @param function void (*function)(char** param, u8 parCnt)
 * @param description Command description
 * @param hidden Hide Command in CLI
 * @return bool true if Command added successfully
*/
bool EspToolkit::commandAdd(const char* name,void (*function)(void*,void (*)(char*), char** param, u8 parCnt),void* ctx,const char* description,bool hidden){
    lock();
    AOS_CMD* b = new AOS_CMD{name,function,ctx,description,hidden,nullptr};
    if(aos_cmd == nullptr){
        aos_cmd = b;
    }else{
        AOS_CMD* i{aos_cmd};
        while(i->aos_cmd != nullptr){
            if(!strcmp(i->name,name)){
                i->function     = function;
                i->description  = description;
                i->hidden       = hidden;
                delete b;
                unlock();
                return true;
            };    
            i = i->aos_cmd;
        };
        i->aos_cmd = b;
    }
    unlock();
    return false;
};

/**
 * List aviable Commands on CLI
 * 
 * @param filter Filter on Command name
*/
void EspToolkit::commandList(const char* filter,void (*reply)(char*)){
    reply((char*)textCommands);
    reply(EOL);
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){
            snprintf(OUT,LONG, "%-20s %s %s",i->name,i->description,EOL);reply(OUT);
        }
        i = i->aos_cmd;
    };
}

/**
 * Display Command manual on CLI
 * 
 * @param filter Filter by Command name
*/
void EspToolkit::commandMan(const char* name, void (*reply)(char*)){
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!strcmp(i->name,name)){
            reply((char*)i->description);
            return;
        }
        i = i->aos_cmd;
    };
    return;
}

/**
 * Run Command with Parameter
 * 
 * @param name Name of the Command
 * @param param Parameter
 * @param paramCnt Parameter count
*/
bool EspToolkit::commandCall(const char* name,void (*reply)(char*), char** param, u8 parCnt){
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!strncmp(i->name,name,SHORT)){
            (*(i->function))(i->ctx, reply, param, parCnt);
            return true;
        }
        i = i->aos_cmd;
    };
    return false;
}

/**
 * 
 * Parse Command From string an run
 *
*/
void EspToolkit::commandParseAndCall(char* ca, void (*reply)(char*)){
    u8          parCnt{0};
    char*       param[SHORT]{NULL};
    char        search{' '};
    unsigned    s{0};
    for(unsigned i{0};i<strlen(ca);i++){
        if(ca[i] == 0) break;
        while((ca[i] == '"' || ca[i] == ' ') && ca[i] != 0) search = ca[i++];
        s = i;
        while(ca[i] != search && ca[i] != 0) i++;
        char* buffer = (char*)malloc(i-s+1);
        for(unsigned j{0};j<(i-s);j++) buffer[j] = ca[s+j];
        buffer[i-s] = 0;param[parCnt++] = buffer;
    }
    if(parCnt>0) if(!commandCall(param[0],reply,param,parCnt)){
        reply((char*)textCommandNotFound);
        reply(EOL);
    }
    for(u8 i{0};i<parCnt;i++) delete param[i];
};

//Variables
bool EspToolkit::variableAdd(const char* n,bool& v,const char* d,bool h,bool p)  {return _variableAdd(n,&v,d,h,p,AOS_DT_BOOL);};
bool EspToolkit::variableAdd(const char* n,int& v,const char* d,bool h,bool p)   {return _variableAdd(n,&v,d,h,p,AOS_DT_INT);};
bool EspToolkit::variableAdd(const char* n,double& v,const char* d,bool h,bool p){return _variableAdd(n,&v,d,h,p,AOS_DT_DOUBLE);};
bool EspToolkit::variableAdd(const char* n,String& v,const char* d,bool h,bool p){return _variableAdd(n,&v,d,h,p,AOS_DT_STRING);};
bool EspToolkit::_variableAdd(const char* name,void* value,const char* description,bool hidden,bool protect,AOS_DT aos_dt){
    if(isBegin){
        std::cout << "call variableAdd() before begin() !" << std::endl;
        return false;
    }
    lock();
    AOS_VAR* b = new AOS_VAR{name,value,description,hidden,protect,aos_dt,nullptr};
    if(aos_var == nullptr){
        aos_var = b;
    }else{
        AOS_VAR* i{aos_var};
        while(i->aos_var != nullptr){
            if(!strcmp(i->name,name)){
                i->value        = value;
                i->description  = description;
                i->hidden       = hidden;
                i->protect      = protect;
                i->aos_dt       = aos_dt;
                delete b;
                unlock();
                return true;
            }
            i = i->aos_var;
        };
        i->aos_var = b;
    } 
    unlock();
    return false;
};
void EspToolkit::variableList(const char* filter, void (*reply)(char*)){
    reply((char*)textVariables);
    reply(EOL);
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){ 
            if(i->aos_dt==AOS_DT_BOOL)   snprintf(OUT,LONG,"%-30s : %-20s\t%s %s %s", i->name,*(bool*)(i->value) ? "true" : "false",i->description,(i->protect ? "(Protected)":""),EOL);
            if(i->aos_dt==AOS_DT_INT)    snprintf(OUT,LONG,"%-30s : %-20d\t%s %s %s", i->name,*(int*)(i->value),i->description,(i->protect ? "(Protected)":""),EOL);
            if(i->aos_dt==AOS_DT_DOUBLE) {char str_temp[SHORT];dtostrf(*(double*)(i->value), 4, 2, str_temp);snprintf(OUT,LONG,"%-30s : %-20s\t%s %s %s", i->name,str_temp,i->description,(i->protect ? "(Protected)":""),EOL);};
            if(i->aos_dt==AOS_DT_STRING) snprintf(OUT,LONG,"%-30s : %-20s\t%s %s %s", i->name,(*(String*)(i->value)).c_str(),i->description,(i->protect ? "(Protected)":""),EOL);
            reply(OUT);
        }
        i = i->aos_var;
    };
}
bool EspToolkit::variableSet(const char* name,char* value){
    lock();
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!strcmp(i->name,name)){
            if(i->protect) return false;
            if(i->aos_dt==AOS_DT_BOOL)      *(bool*)(i->value)   = !value ? false   : (!strcmp(value,"1") || !strcmp(value,"true")) ? true : false;
            if(i->aos_dt==AOS_DT_INT)       *(int*)(i->value)    = !value ? 0       : atoi(value);
            if(i->aos_dt==AOS_DT_DOUBLE)    *(double*)(i->value) = !value ? 0.0     : atof(value); 
            if(i->aos_dt==AOS_DT_STRING)    *(String*)(i->value) = !value ? 0       : value;
            unlock();
            return true;
        }
        i = i->aos_var;
    };
    unlock();
    return false;
};
void* EspToolkit::variableGet(const char* name){
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!strcmp(i->name,name)) return i->value;
        i = i->aos_var;
    };
    return nullptr;
};
void EspToolkit::variableLoad(bool save){
    lock();
    if(!isBegin) isBegin = true;
    AOS_VAR* i{aos_var};unsigned int p{0};
    while(i != nullptr){
            if(i->aos_dt==AOS_DT_BOOL){
                if(save)EEPROM.put(p,*(bool*)(i->value));
                else    EEPROM.get(p,*(bool*)(i->value));
                p+=sizeof(bool);
            } 
            if(i->aos_dt==AOS_DT_INT){
                if(save)EEPROM.put(p,*(int*)(i->value));
                else    EEPROM.get(p,*(int*)(i->value));
                p+=sizeof(int);
            } 
            if(i->aos_dt==AOS_DT_DOUBLE){
                if(save)EEPROM.put(p,*(double*)(i->value));
                else    EEPROM.get(p,*(double*)(i->value));
                p+=sizeof(double);
            }
            if(i->aos_dt==AOS_DT_STRING){
                if(save){
                    for(unsigned int s{0};s<(*(String*)(i->value)).length();s++,p++){
                        EEPROM.write(p,(*(String*)(i->value))[s]);
                    }
                    EEPROM.write(p,(char)0);p++;
                }else{
                    *(String*)(i->value) = "";
                    while(true){
                        char b = EEPROM.read(p);p++;
                        if((u8)b == (char)0) break;
                        *(String*)(i->value)+=b; 
                        if((*(String*)(i->value)).length() >= LONG) break;
                    }
                }
            }
            if(((date != date_temp && autoReset) || date == "!") && !save){
                ESP_LOGE(EVT_TK_PREFIX, "RESET");
                date=date_temp;
                variableLoad(true);
                return;
            }
        i = i->aos_var;
    };
    usedEeprom = p;
    if(save) EEPROM.commit();
    unlock();
};

//TOOLS
double EspToolkit::mapVal(double x, int in_min, int in_max, int out_min, int out_max)
{
    double r = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if(r > out_max) r = out_max;
    if(r < out_min) r = out_min;
    return r;
}

void EspToolkit::lock(bool isr){
    if(isr)
        xSemaphoreTake(xBinarySemaphore,portMAX_DELAY);
    else
        xSemaphoreTakeFromISR(xBinarySemaphore,0);
}

void EspToolkit::unlock(bool isr){
    if(isr)
        xSemaphoreGive(xBinarySemaphore);
    else
        xSemaphoreGiveFromISR(xBinarySemaphore,0);  
}

void EspToolkit::variablesAddDefault(){
    variableAdd("sys/date",     date,"",true,true);
    variableAdd("sys/hostname", hostname,"");
    variableAdd("sys/password", password,"");
};

void EspToolkit::commandAddDefault(){
    commandAdd("gpio",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        if(parCnt == 2){
        pinMode(atoi(param[1]),INPUT);
            if(digitalRead(atoi(param[1])))
                reply("1"); 
            else
                reply("0"); 
        }else if(parCnt == 3){
        pinMode(atoi(param[1]),OUTPUT);
        digitalWrite(atoi(param[1]),atoi(param[2]));
        }else{
        reply((char*)textInvalidParameter);
        reply(EOL);
        commandMan("gpio",reply);
        return;
        }
    },NULL,"🖥  gpio [pin] [0|1]");

    commandAdd("help",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        if(parCnt == 1)
            commandList(NULL,reply);
        if(parCnt == 2)
            commandList(param[1],reply);
    },NULL,"",true);

    commandAdd("get",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        if(parCnt == 2)
            variableList(param[1],reply);
        else
            variableList(NULL,reply);
    },NULL,"🖥  get [filter]");

    commandAdd("set",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        if(parCnt < 2 || parCnt > 3){
            reply((char*)textInvalidParameter);
            reply(EOL);
            commandMan("set",reply);
            return;
        }
        if(variableSet(param[1],param[2])){
            variableLoad(true);
            reply(textOk);
            reply(EOL);
        }else{
            reply((char*)textNotFound); 
            reply(EOL);
        } 
    },NULL,"🖥  set [par] [val]");

    commandAdd("clear",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        reply((char*)textEscClear);
        reply(EOL);
    },NULL,"",true);

    commandAdd("lock",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        shell.lock();
    },NULL,"🖥");

    commandAdd("reboot",[](void*, void (*reply)(char*), char** param,u8 parCnt){
        ESP.restart();
    },NULL,"🖥");

    commandAdd("reset",[](void*c , void (*reply)(char*), char** param,u8 parCnt){
        date = "!";
        variableLoad(true);
        commandCall("reboot",NULL);
    },NULL,"🖥");

    commandAdd("status",[](void* c, void (*reply)(char*), char** param,u8 parCnt){
        reply("🖥  System:");
        snprintf(OUT,LONG,"%-20s : %s %s","IDF Version",esp_get_idf_version(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %s %s","FIRMWARE",firmware.c_str(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %s %s","COMPILED",date.c_str(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %d Bytes FREE %s","HEAP",esp_get_free_heap_size(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %d Bytes MIN FREE %s","HEAP",esp_get_minimum_free_heap_size(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %d Bytes USED %s","EERPOM",usedEeprom,EOL);reply(OUT);
        snprintf(OUT,LONG,"%-20s : %f Stunden %s","UPTIME",(double)millis()/1000.0/60.0/60.0,EOL);reply(OUT);
    },NULL,"🖥");

    commandAdd("tasks", [](void* c, void (*reply)(char*), char** param,u8 parCnt){
        timer.printTasks(reply);
    },NULL,"🖥");
};

#endif