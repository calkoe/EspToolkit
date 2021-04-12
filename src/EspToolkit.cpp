#if defined ESP32

#include "EspToolkit.h"

//Global
PostOffice<std::string> EspToolkit::events(100);
Uart                    EspToolkit::uart(&events,(char*)EVT_TK_COMMAND,(char*)EVT_TK_BROADCAST);
SyncTimer               EspToolkit::timer;
Button                  EspToolkit::button(&events);
char*                   EspToolkit::EOL{(char*)"\r\n"};
bool                    EspToolkit::isBegin{false};
EspToolkit::AOS_CMD*    EspToolkit::aos_cmd{nullptr};
EspToolkit::AOS_VAR*    EspToolkit::aos_var{nullptr};
char                    EspToolkit::OUT[LONG]{0};
bool                    EspToolkit::status[5]{true};
int                     EspToolkit::logLevel{1};
int                     EspToolkit::watchdog{10};
std::string             EspToolkit::date{__DATE__ " " __TIME__};
std::string             EspToolkit::hostname{"EspToolkit"};
std::string             EspToolkit::password{"tk"};
std::string             EspToolkit::firmware{"-"};

EspToolkit::EspToolkit(){

    // Status LED
    for(uint8_t i{0};i<5;i++) status[i] = true;
    status[STATUS_BIT_SYSTEM] = false;
    if(STATUSLED>=0){
        xTaskCreate([](void* arg){
            EspToolkit* _this = (EspToolkit*) arg;
            gpio_reset_pin((gpio_num_t)STATUSLED);
            gpio_set_direction((gpio_num_t)STATUSLED, GPIO_MODE_OUTPUT);
            while(true){
                bool allSet{true};
                for(uint8_t i{0};i<5;i++) if(!_this->status[i]) allSet = false;
                if(allSet){
                    gpio_set_level((gpio_num_t)STATUSLED,STATUSLEDON);
                    continue;  
                }
                for(uint8_t i{0};i<5;i++){
                    if(_this->status[i]) gpio_set_level((gpio_num_t)STATUSLED,STATUSLEDON);
                    vTaskDelay(150);
                    gpio_set_level((gpio_num_t)STATUSLED,!STATUSLEDON);
                    vTaskDelay(150);
                }
                vTaskDelay(1000);
            }
        }, "statusled", 2048, this, 1, NULL);
    }
    
    // BUTTON RESET
    button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,5000,(char*)"bootbutton5000ms");
    events.on(EVT_TK_THREAD,"bootbutton5000ms",[](void* ctx, void* arg){
        if(!*(bool*)arg){
            ESP_LOGE(EVT_TK_PREFIX, "BUTTON RESET");
            variableLoad(false,true);
            esp_restart();
        }
    },this);

    // Command Parser
    events.on(EVT_TK_THREAD,EVT_TK_COMMAND,[](void* ctx, void* arg){
        EspToolkit* _this = (EspToolkit*) arg;
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(char* str);
        };
        simple_cmd_t simple_cmd = *(simple_cmd_t*) arg;
        _this->commandParseAndCall(simple_cmd.payload,simple_cmd.reply);
    },this);


    // Toolkit
    variablesAddDefault();
    commandAddDefault();

}

void EspToolkit::begin(){

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // Initialize Variables
    variableLoad();

    // Initialize Logging
    switch(logLevel){
        case 0:
            esp_log_level_set("*", ESP_LOG_ERROR);
            break;
        case 1:
            esp_log_level_set("*", ESP_LOG_WARN);
            break;
        case 2:
            esp_log_level_set("*", ESP_LOG_INFO);
            break;
        case 3:
            esp_log_level_set("*", ESP_LOG_DEBUG);
            break;
        case 4:
            esp_log_level_set("*", ESP_LOG_VERBOSE);
            break;
    }

    // Initialize Watchdog
    if(watchdog){
        esp_task_wdt_init(watchdog, true); 
        esp_task_wdt_add(NULL); 
    }

    // Setup Shell
    uart.setPassword((char*)password.c_str());


    //Sys Status
    status[STATUS_BIT_SYSTEM] = true;

}

void EspToolkit::loop(){
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
 * @param function void (*function)(char** param, uint8_t parCnt)
 * @param description Command description
 * @param hidden Hide Command in CLI
 * @return bool true if Command added successfully
*/
bool EspToolkit::commandAdd(const char* name,void (*function)(void*,void (*)(char*), char** param, uint8_t parCnt),void* ctx,const char* description,bool hidden){
    
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
                
                return true;
            };    
            i = i->aos_cmd;
        };
        i->aos_cmd = b;
    }
    
    return false;
};

/**
 * List aviable Commands on CLI
 * 
 * @param filter Filter on Command name
*/
void EspToolkit::commandList(const char* filter,void (*reply)(char*)){
    reply((char*)"Commands:");
    reply(EOL);
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){
            snprintf(OUT,LONG, "%-30s %s %s",i->name,i->description,EOL);reply(OUT);
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
bool EspToolkit::commandCall(const char* name,void (*reply)(char*), char** param, uint8_t parCnt){
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
    uint8_t          parCnt{0};
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
    if(parCnt>0){
        if(!commandCall(param[0],reply,param,parCnt)){
            reply((char*)"Command not found! Try 'help' for more information.");reply(EOL);
        }
    }
    reply((char*)"ESPToolkit:/>");
    for(uint8_t i{0};i<parCnt;i++) delete param[i];
};

//Variables
bool EspToolkit::variableAdd(const char* n,bool& v,const char* d,bool h,bool p)  {return _variableAdd(n,&v,d,h,p,AOS_DT_BOOL);};
bool EspToolkit::variableAdd(const char* n,int& v,const char* d,bool h,bool p)   {return _variableAdd(n,&v,d,h,p,AOS_DT_INT);};
bool EspToolkit::variableAdd(const char* n,double& v,const char* d,bool h,bool p){return _variableAdd(n,&v,d,h,p,AOS_DT_DOUBLE);};
bool EspToolkit::variableAdd(const char* n,std::string& v,const char* d,bool h,bool p){return _variableAdd(n,&v,d,h,p,AOS_DT_STRING);};
bool EspToolkit::_variableAdd(const char* name,void* value,const char* description,bool hidden,bool protect,AOS_DT aos_dt){
    if(isBegin){
        std::cout << "call variableAdd() before begin() !" << std::endl;
        return false;
    }
    
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
                
                return true;
            }
            i = i->aos_var;
        };
        i->aos_var = b;
    } 
    
    return false;
};
void EspToolkit::variableList(const char* filter, void (*reply)(char*)){
    reply((char*)"Variables:");
    reply(EOL);
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){ 
            if(i->aos_dt==AOS_DT_BOOL)   snprintf(OUT,LONG,"%-30s : %-40s\t%s %s %s", i->name,*(bool*)(i->value) ? "true" : "false",i->description,(i->protect ? "(Protected)":""),EOL);
            if(i->aos_dt==AOS_DT_INT)    snprintf(OUT,LONG,"%-30s : %-40d\t%s %s %s", i->name,*(int*)(i->value),i->description,(i->protect ? "(Protected)":""),EOL);
            if(i->aos_dt==AOS_DT_DOUBLE) snprintf(OUT,LONG,"%-30s : %-40f\t%s %s %s", i->name,*(double*)(i->value),i->description,(i->protect ? "(Protected)":""),EOL);
            if(i->aos_dt==AOS_DT_STRING) snprintf(OUT,LONG,"%-30s : %-40s\t%s %s %s", i->name,(*(std::string*)(i->value)).c_str(),i->description,(i->protect ? "(Protected)":""),EOL);
            reply(OUT);
        }
        i = i->aos_var;
    };
}
bool EspToolkit::variableSet(const char* name,char* value){
    
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!strcmp(i->name,name)){
            if(i->protect) return false;
            if(i->aos_dt==AOS_DT_BOOL)      *(bool*)(i->value)   = !value ? false   : (!strcmp(value,"1") || !strcmp(value,"true")) ? true : false;
            if(i->aos_dt==AOS_DT_INT)       *(int*)(i->value)    = !value ? 0       : atoi(value);
            if(i->aos_dt==AOS_DT_DOUBLE)    *(double*)(i->value) = !value ? 0.0     : atof(value); 
            if(i->aos_dt==AOS_DT_STRING)    *(std::string*)(i->value) = !value ? 0       : value;
            return true;
        }
        i = i->aos_var;
    };
    
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
void EspToolkit::variableLoad(bool save, bool reset){
    if(!isBegin) isBegin = true;
    AOS_VAR* i{aos_var};
    // Open NVS
    ESP_LOGI(EVT_TK_PREFIX,"Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle my_handle;
    esp_err_t ret;
    ret = nvs_open("tkstorage", NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(EVT_TK_PREFIX,"Opening Non-Volatile Storage (NVS) handle... ",esp_err_to_name(ret));
    } else {
        ESP_LOGI(EVT_TK_PREFIX,"Opening Non-Volatile Storage (NVS) Done!");
    }
    if(reset){
        nvs_erase_all(my_handle);
    }
    while(i != nullptr){
            if(i->aos_dt==AOS_DT_BOOL){
                size_t size = sizeof(bool);
                if(save) nvs_set_blob(my_handle, i->name, i->value, size);
                else     nvs_get_blob(my_handle, i->name, i->value, &size);
            } 
            if(i->aos_dt==AOS_DT_INT){
                size_t size = sizeof(int);
                if(save) nvs_set_blob(my_handle, i->name, i->value, size);
                else     nvs_get_blob(my_handle, i->name, i->value, &size);
            } 
            if(i->aos_dt==AOS_DT_DOUBLE){
                size_t size = sizeof(double);
                if(save) nvs_set_blob(my_handle, i->name, i->value, size);
                else     nvs_get_blob(my_handle, i->name, i->value, &size);
            }
            if(i->aos_dt==AOS_DT_STRING){
                if(save){
                    nvs_set_str(my_handle, i->name, (*(std::string*)(i->value)).c_str());
                }else{
                    char ca[LONG]{0};
                    size_t size =  LONG;
                    if(nvs_get_str(my_handle, i->name, ca, &size) == ESP_OK){
                        *(std::string*)(i->value) = ca;
                    };
                }
            }
        i = i->aos_var;
    };
    if(save) nvs_commit(my_handle);
    nvs_close(my_handle);
};

//TOOLS
double EspToolkit::mapVal(double x, int in_min, int in_max, int out_min, int out_max)
{
    double r = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if(r > out_max) r = out_max;
    if(r < out_min) r = out_min;
    return r;
}

void EspToolkit::variablesAddDefault(){
    variableAdd("sys/date",     date,"",true,true);
    variableAdd("sys/hostname", hostname,"");
    variableAdd("sys/password", password,"");
    variableAdd("sys/loglevel", logLevel,"0-Error | 1-Warning | 2-info | 3-Debug | 4-Verbose");
};

void EspToolkit::commandAddDefault(){

    commandAdd("gpio",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        if(parCnt == 2){
            gpio_num_t gpio = (gpio_num_t)atoi(param[1]);
            gpio_reset_pin(gpio);
            gpio_set_direction(gpio, GPIO_MODE_INPUT);reply(EOL);
            if(gpio_get_level(gpio)){
                reply((char*)"HIGH");
                reply(EOL);
            }else{
                reply((char*)"LOW");
                reply(EOL);
            }
        }else if(parCnt == 3){
            gpio_num_t gpio = (gpio_num_t)atoi(param[1]);
            gpio_reset_pin(gpio);
            gpio_set_direction(gpio,GPIO_MODE_OUTPUT);
            gpio_set_level(gpio,atoi(param[2]));
        }else{
            reply((char*)"Invalid parameter!");
            reply(EOL);
            commandMan("gpio",reply);
            return;
        }
    },NULL,"🖥  gpio [pin] [0|1]");

    commandAdd("help",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        if(parCnt == 1)
            commandList(NULL,reply);
        if(parCnt == 2)
            commandList(param[1],reply);
    },NULL,"",true);

    commandAdd("get",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        if(parCnt == 2)
            variableList(param[1],reply);
        else
            variableList(NULL,reply);
    },NULL,"🖥  get [filter]");

    commandAdd("set",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        if(parCnt < 2 || parCnt > 3){
            reply((char*)"Invalid parameter!");
            reply(EOL);
            commandMan("set",reply);
            return;
        }
        if(variableSet(param[1],param[2])){
            variableLoad(true);
            reply((char*)"ok");
            reply(EOL);
        }else{
            reply((char*)"Parameter not found!");
            reply(EOL);
        } 
    },NULL,"🖥  set [par] [val]");

    commandAdd("clear",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        reply((char*)"\033[2J\033[11H");
        reply(EOL);
    },NULL,"",true);

    commandAdd("lock",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){},NULL,"🖥 lock current shell");

    commandAdd("reboot",[](void*, void (*reply)(char*), char** param,uint8_t parCnt){
        esp_restart();
    },NULL,"🖥");

    commandAdd("reset",[](void*c , void (*reply)(char*), char** param,uint8_t parCnt){
        variableLoad(false,true);
        esp_restart();
    },NULL,"🖥");

    commandAdd("status",[](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        reply((char*)"🖥  System:");
        reply(EOL);
        snprintf(OUT,LONG,"%-30s : %s %s","IDF Version",esp_get_idf_version(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s %s","FIRMWARE",firmware.c_str(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s %s","COMPILED",date.c_str(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d Bytes FREE %s","HEAP",esp_get_free_heap_size(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d Bytes MIN FREE %s","HEAP",esp_get_minimum_free_heap_size(),EOL);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %f Hours %s","UPTIME",(double)esp_timer_get_time()/1000.0/1000.0/60.0/60.0,EOL);reply(OUT);
    },NULL,"🖥");

    commandAdd("tasks", [](void* c, void (*reply)(char*), char** param,uint8_t parCnt){
        timer.printTasks(reply);
    },NULL,"🖥");
};

#endif