#include "EspToolkit.h"

//Global
PostOffice<std::string> EspToolkit::events(100);
Uart                    EspToolkit::uart(&events,EVT_TK_COMMAND,EVT_TK_BROADCAST);
SyncTimer               EspToolkit::timer;
Button                  EspToolkit::button(&events);
bool                    EspToolkit::isBegin{false};
EspToolkit::AOS_CMD*    EspToolkit::aos_cmd{nullptr};
EspToolkit::AOS_VAR*    EspToolkit::aos_var{nullptr};
char                    EspToolkit::OUT[LONG]{0};
bool                    EspToolkit::status[5]{true};
int                     EspToolkit::cpuFreq{2};
int                     EspToolkit::logLevel{0};
int                     EspToolkit::watchdog{60};
std::string             EspToolkit::date{__DATE__ " " __TIME__};
std::string             EspToolkit::hostname{"EspToolkit"};
std::string             EspToolkit::password{"tk"};
bool                    EspToolkit::locked{false};
std::string             EspToolkit::firmware{FIRMWARE};

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
                vTaskDelay(2000);
            }
        }, "statusled", 2048, this, 1, NULL);
    }
    
    // BUTTON RESET
    button.add((gpio_num_t)BOOTBUTTON,GPIO_FLOATING,5000,"bootbutton5000ms");
    events.on(0,"bootbutton5000ms",[](void* ctx, void* arg){
        if(!*(bool*)arg){
            ESP_LOGE("TOOLKIT", "BUTTON RESET");
            variableLoad(false,true);
            esp_restart();
        }
    },this);

    // Command Parser
    events.on(0,EVT_TK_COMMAND,[](void* ctx, void* arg){
        EspToolkit* _this = (EspToolkit*) arg;
        struct simple_cmd_t{
            char* payload;
            void  (*reply)(const char* str);
        };
        simple_cmd_t simple_cmd = *(simple_cmd_t*) arg;
        //printf("Command Parser got: %s %s",simple_cmd.payload,EOL);
        _this->commandParseAndCall(simple_cmd.payload,simple_cmd.reply);
        free((void*)simple_cmd.payload);
    },this);


    // Toolkit
    variablesAddDefault();
    commandAddDefault();

}

void EspToolkit::begin(){

    // Initialize NVS
    ESP_LOGI("TOOLKIT", "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // Initialize SPIFFS
    ESP_LOGI("TOOLKIT", "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 10,
      .format_if_mount_failed = true
    };
    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("TOOLKIT", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("TOOLKIT", "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE("TOOLKIT", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }

    // Initialize Variables
    ESP_LOGI("TOOLKIT", "Initializing Variables");
    variableLoad();

    // Initialize CPU
    ESP_LOGI("TOOLKIT", "Initializing CPU");
    esp_pm_config_esp32_t pm_config;
    switch(cpuFreq){
        case 0:
            pm_config = {
                .max_freq_mhz = 80,
                .min_freq_mhz = 80,
                .light_sleep_enable = false
            };
            break;
        case 1:
            pm_config = {
                .max_freq_mhz = 160,
                .min_freq_mhz = 160,
                .light_sleep_enable = false
            };
            break;
        case 2:
            pm_config = {
                .max_freq_mhz = 240,
                .min_freq_mhz = 240,
                .light_sleep_enable = false
            };
            break;
        default:
            pm_config = {
                .max_freq_mhz = 240,
                .min_freq_mhz = 240,
                .light_sleep_enable = false
            };
            break;
    }
    esp_pm_configure(&pm_config);


    // Initialize Logging
    ESP_LOGI("TOOLKIT", "Initializing Logging");
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
        default:
            esp_log_level_set("*", ESP_LOG_ERROR);
            break;
    }

    // Initialize Watchdog
    ESP_LOGI("TOOLKIT", "Initializing Watchdog");
    if(watchdog){
        esp_task_wdt_init(watchdog, true); 
        esp_task_wdt_add(NULL); 
    }

    //Sys Status
    ESP_LOGI("TOOLKIT", "Initializing Sys Status");
    status[STATUS_BIT_SYSTEM] = true;

}

void EspToolkit::loop(){
    if(watchdog) esp_task_wdt_reset(); 
    timer.loop();
    events.loop(0);
};

/**
 *  Emits a Message event to EVT_TK_BROADCAST 
 */
void EspToolkit::broadcast(const char* msg){
    events.emit(EVT_TK_BROADCAST,(void*)msg,strlen(msg)+1);
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
bool EspToolkit::commandAdd(const char* name,void (*function)(void*,void (*)(const char*), char** param, uint8_t parCnt),void* ctx,const char* description,bool hidden){
    
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
void EspToolkit::commandList(const char* filter,void (*reply)(const char*)){
    reply("\r\nCommands:\r\n");
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){
            snprintf(OUT,LONG, "%-30s %s\r\n",i->name,i->description);reply(OUT);
        }
        i = i->aos_cmd;
    };
}

/**
 * Display Command manual on CLI
 * 
 * @param filter Filter by Command name
*/
void EspToolkit::commandMan(const char* name, void (*reply)(const char*)){
    AOS_CMD* i{aos_cmd};
    while(i != nullptr){
        if(!strcmp(i->name,name)){
            reply(i->description);reply("\r\n");
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
bool EspToolkit::commandCall(const char* name,void (*reply)(const char*), char** param, uint8_t parCnt){

    // Locking Meschanism
    if(locked){
        if(password.empty() || !strncmp(name,password.c_str(),SHORT)){
            locked=false;
            reply("SYSTEM UNLOCKED!\r\n");
            variableLoad(true);
        }else{
            reply("SYSTEM LOCKED! Type Password\r\n");
            vTaskDelay(1000);
        }
        return true;
    }

    AOS_CMD* i{aos_cmd};
    while(!locked && i != nullptr){
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
void EspToolkit::commandParseAndCall(const char* ca, void (*reply)(const char*)){
    uint8_t     parCnt{0};
    char*       param[SHORT]{NULL};
    char        search{' '};
    unsigned    s{0};
    for(unsigned i{0};i<strlen(ca);i++){
        if(ca[i] == '\0' || ca[i] == '\r' || ca[i] == '\n') break;
        while((ca[i] == '"' || ca[i] == ' ') && ca[i] != '\0' && ca[i] != '\r' && ca[i] != '\n') search = ca[i++];
        s = i;
        while(ca[i] != search && ca[i] != '\0' && ca[i] != '\r' && ca[i] != '\n') i++;
        char* buffer = (char*)malloc(i-s+1);
        for(unsigned j{0};j<(i-s);j++) buffer[j] = ca[s+j];
        buffer[i-s] = 0;param[parCnt++] = buffer;
    }
    if(parCnt>0){
        if(!commandCall(param[0],reply,param,parCnt)){
            reply("Command not found! Try 'help' for more information.\r\n");
        }
    }
    reply("ESPToolkit:/>");
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
void EspToolkit::variableList(const char* filter, void (*reply)(const char*)){
    reply("Variables:\r\n");
    AOS_VAR* i{aos_var};
    while(i != nullptr){
        if(!i->hidden && (!filter || strstr(i->name, filter))){ 
            if(i->aos_dt==AOS_DT_BOOL)   snprintf(OUT,LONG,"%-30s : %-40s\t%s %s\r\n", i->name,*(bool*)(i->value) ? "true" : "false",i->description,(i->protect ? "(Protected)":""));
            if(i->aos_dt==AOS_DT_INT)    snprintf(OUT,LONG,"%-30s : %-40d\t%s %s\r\n", i->name,*(int*)(i->value),i->description,(i->protect ? "(Protected)":""));
            if(i->aos_dt==AOS_DT_DOUBLE) snprintf(OUT,LONG,"%-30s : %-40f\t%s %s\r\n", i->name,*(double*)(i->value),i->description,(i->protect ? "(Protected)":""));
            if(i->aos_dt==AOS_DT_STRING) snprintf(OUT,LONG,"%-30s : %-40s\t%s %s\r\n", i->name,(*(std::string*)(i->value)).c_str(),i->description,(i->protect ? "(Protected)":""));
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
            if(i->aos_dt==AOS_DT_STRING)    *(std::string*)(i->value) = !value ? "" : value;
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
    ESP_LOGI("TOOLKIT","Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle my_handle;
    esp_err_t ret;
    ret = nvs_open(NVS_DEFAULT_PART_NAME, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("TOOLKIT","Opening Non-Volatile Storage (NVS) handle...");
        //ESP_LOGE("TOOLKIT",esp_err_to_name(ret));
    } else {
        ESP_LOGI("TOOLKIT","Opening Non-Volatile Storage (NVS) Done!");
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

void EspToolkit::variablesAddDefault(){
    variableAdd("sys/hostname", hostname,   "🖥  System Hostname");
    variableAdd("sys/password", password,   "🖥  System Password");
    variableAdd("sys/locked",   locked,     "🖥  Locked");
    variableAdd("sys/cpuFreq",  cpuFreq,    "🖥  CPU Speed: 0=80MHz | 1=160MHz | 2=240MHz");
    variableAdd("sys/loglevel", logLevel,   "🖥  Loglevel: 0=Error | 1=Warning | 2=info | 3=Debug | 4=Verbose");
};

void EspToolkit::commandAddDefault(){

    commandAdd("gpio",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt == 2){
            gpio_num_t gpio = (gpio_num_t)atoi(param[1]);
            gpio_reset_pin(gpio);
            gpio_set_direction(gpio, GPIO_MODE_INPUT);
            if(gpio_get_level(gpio)){
                reply("1\r\n");
            }else{
                reply("0\r\n");
            }
        }else if(parCnt == 3){
            gpio_num_t gpio = (gpio_num_t)atoi(param[1]);
            gpio_reset_pin(gpio);
            gpio_set_direction(gpio,GPIO_MODE_OUTPUT);
            gpio_set_level(gpio,atoi(param[2]));
        }else{
            reply("Invalid parameter!\r\n");
            commandMan("gpio",reply);
            return;
        }
    },NULL,"🖥  gpio [pin] [0|1]");

    commandAdd("help",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt == 1)
            commandList(NULL,reply);
        if(parCnt == 2)
            commandList(param[1],reply);
    },NULL,"",true);

    commandAdd("get",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt == 2)
            variableList(param[1],reply);
        else
            variableList(NULL,reply);
    },NULL,"🖥  get [filter]");

    commandAdd("set",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt < 2 || parCnt > 3){
            reply("Invalid parameter!\r\n");
            commandMan("set",reply);
            return;
        }
        if(variableSet(param[1],param[2])){
            variableLoad(true);
            reply("ok\r\n");
        }else{
            reply("Parameter not found!\r\n");
        } 
    },NULL,"🖥  set [par] [val]");

    commandAdd("clear",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("\033[2J\033[11H\r\n");
    },NULL,"",true);

    commandAdd("lock",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("SYSTEM LOCKED!\r\n");
        locked=true;
        variableLoad(true);
    },NULL,"🖥  lock current shell");

    commandAdd("reboot",[](void*, void (*reply)(const char*), char** param,uint8_t parCnt){
        esp_restart();
    },NULL,"🖥  Reboot");

    commandAdd("reset",[](void*c , void (*reply)(const char*), char** param,uint8_t parCnt){
        variableLoad(false,true);
        esp_restart();
    },NULL,"🖥  Reset Values stored in NVS to defaults");

    commandAdd("status",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("🖥  System:\r\n");
        snprintf(OUT,LONG,"%-30s : %s\r\n","IDF Version",esp_get_idf_version());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","FIRMWARE",firmware.c_str());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","COMPILED",date.c_str());reply(OUT);
        //esp_pm_config_esp32_t pm_config;
        //esp_pm_get_configuration(&pm_config);
        //snprintf(OUT,LONG,"%-30s : %d MHz\r\n","CPU Frequency",getCpuFrequencyMhz());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d Bytes FREE, %d Bytes MIN FREE\r\n","HEAP",esp_get_free_heap_size(),esp_get_minimum_free_heap_size());reply(OUT);
        nvs_stats_t nvs_stats;
        esp_err_t ret = nvs_get_stats(NVS_DEFAULT_PART_NAME, &nvs_stats);
        if(ret != ESP_OK){
            ESP_LOGE("TOOLKIT", "Failed to get NVS partition information (%s)", esp_err_to_name(ret));
        }else{
            snprintf(OUT,LONG,"%-30s : %d total_entries, %d free_entries, %d used_entries, %d namespace_count\r\n","NVS",nvs_stats.total_entries,nvs_stats.free_entries,nvs_stats.used_entries,nvs_stats.namespace_count);reply(OUT);
        }
        size_t total = 0, used = 0;
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE("TOOLKIT", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        } else {
            esp_spiffs_info("/", &total, &used);
            snprintf(OUT,LONG,"%-30s : %d Bytes total, %d Bytes used\r\n","SPIFFS",total,used);reply(OUT);
        }
        snprintf(OUT,LONG,"%-30s : %f Hours\r\n","UPTIME",(double)esp_timer_get_time()/1000.0/1000.0/60.0/60.0);reply(OUT);
        time_t rawtime;
        struct tm * timeinfo;
        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        snprintf(OUT,LONG,"%-30s : %s","SYSTEM TIME",asctime(timeinfo));reply(OUT);
    },NULL,"🖥  Print system information");

    commandAdd("timers", [](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        timer.printTasks(reply);
    },NULL,"🖥  Print active syncTimers");
};

//TOOLS
double mapVal(double x, int in_min, int in_max, int out_min, int out_max)
{
    double r = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if(r > out_max) r = out_max;
    if(r < out_min) r = out_min;
    return r;
}

std::vector<std::string> split (std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find (delimiter, pos_start)) != std::string::npos) {
        token = s.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back (token);
    }

    res.push_back (s.substr (pos_start));
    return res;
}

char* loadFile(const char* filename){
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        ESP_LOGE("TOOLKIT", "Failed to open file (%s) for reading: ", filename);
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    char* file_content = (char*)malloc(file_size+1);
    rewind(file);
    fread(file_content, sizeof(char), file_size, file);
    fclose(file);
    file_content[file_size] = '\0';
    return file_content;
}