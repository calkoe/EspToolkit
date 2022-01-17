#include "EspToolkit.h"

EspToolkit::EspToolkit(){

    if(EspToolkitInstance){
        ESP_LOGE("TOOLKIT", "ONYL ONE INSTANCE OF EspToolkit IS ALLOWED!");
        return;
    }
    EspToolkitInstance = this;

    // ADD VARIABLES AND ADD COMMANDS
    variablesAddDefault();
    commandAddDefault();

}

void EspToolkit::begin(bool fast){

    if(!fast){

        // Status LED Task
        #ifdef TK_STATUS_PIN
            for(uint8_t i{0};i<5;i++) status[i] = true;
            status[STATUS_BIT_SYSTEM] = false;
            xTaskCreate([](void* arg){
                EspToolkit* _this = (EspToolkit*) arg;
                gpio_reset_pin((gpio_num_t)TK_STATUS_PIN);
                gpio_set_direction((gpio_num_t)TK_STATUS_PIN, GPIO_MODE_OUTPUT);
                while(true){
                    bool allSet{true};
                    for(uint8_t i{0};i<5;i++) if(!_this->status[i]) allSet = false;
                    if(allSet){
                        gpio_set_level((gpio_num_t)TK_STATUS_PIN,TK_STATUS_ACTIVE);
                        continue;  
                    }
                    for(uint8_t i{0};i<5;i++){
                        if(_this->status[i]) gpio_set_level((gpio_num_t)TK_STATUS_PIN,TK_STATUS_ACTIVE);
                        vTaskDelay(150);
                        gpio_set_level((gpio_num_t)TK_STATUS_PIN,!TK_STATUS_ACTIVE);
                        vTaskDelay(150);
                    }
                    vTaskDelay(2000);
                }
            }, "statusled", 2048, this, 0, NULL);
        #endif

        #ifdef TK_BUTTON_PIN
            // REGISTER RESET BUTTON
            button.add((gpio_num_t)TK_BUTTON_PIN,TK_BUTTON_ACTIVE ? GPIO_PULLDOWN_ONLY : GPIO_PULLUP_ONLY,20000,"configButton20000");
            events.on(0,"configButton20000",[](void* ctx, void* arg){
                if(*(bool*)arg == TK_BUTTON_ACTIVE){
                    ESP_LOGE("TOOLKIT", "BUTTON RESET");
                    EspToolkitInstance->variableLoad(false,true);
                    esp_restart();
                }
            },this);
        #endif

        // GENERATE Hostname
        uint8_t baseMac[6];
        esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
        char baseMacChr[20] = {0};
        sprintf(baseMacChr, "EspToolkit-%02X%02X%02X", baseMac[3], baseMac[4], baseMac[5]);
        hostname = std::string(baseMacChr);

    }

    // Initialize and load NVS
    ESP_LOGI("TOOLKIT", "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    variableLoad();

    // Initialize CPU
    ESP_LOGI("TOOLKIT", "Initializing CPU");
    esp_pm_config_esp32_t pm_config;
    switch(cpuFreq){
        case 0:
            setCpuFrequencyMhz(80); 
            break;
        case 1:
            setCpuFrequencyMhz(160); 
            break;
        case 2:
            setCpuFrequencyMhz(240); 
            break;
        default:
            setCpuFrequencyMhz(240); 
            break;
    }

    if(!fast){

        // Initialize SPIFFS
        ESP_LOGI("TOOLKIT", "Initializing SPIFFS");
        esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = false
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

        // Initialize Watchdog
        ESP_LOGI("TOOLKIT", "Initializing Watchdog");
        if(watchdog>0){
            esp_task_wdt_init(watchdog, true); 
            esp_task_wdt_add(NULL); 
        }else{
            esp_task_wdt_deinit();
        }

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

        //Initialize Sys Status
        ESP_LOGI("TOOLKIT", "Initializing Sys Status");
        status[STATUS_BIT_SYSTEM] = true;
    }
}

void EspToolkit::loop(){
    if(watchdog) esp_task_wdt_reset(); 
    timer.loop();
    events.loop(0);
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
        if(password.empty() || !strncmp(name,password.c_str(),64)){
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
        if(!strncmp(i->name,name,64)){
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
void EspToolkit::commandParseAndCall(char* ca, void (*reply)(const char*)){
    uint    i{0};
    char*   args[16];
    uint    argsLen{0};
    while(ca[i]){

        //  Process Spaces
        if(ca[i] == ' '){
            ca[i] = '\0';
            i++;
            while(ca[i] && ca[i] == ' ') i++;
            if(ca[i] != '"'){
                args[argsLen++] = &ca[i];
                i++;
            }
            continue;
        }

        //  Process Marks
        if(ca[i] == '"'){
            i++;
            args[argsLen++] = &ca[i];
            while(ca[i] && ca[i] != '"') i++;
            ca[i] = '\0';
            i++;
            continue;
        }

        //  Set Starting Point
        if(!i) args[argsLen++] = &ca[i];
        i++;

    }

    reply((char*)"\r\n");     

    
    // Call Command
    if(argsLen){
        if(!commandCall(args[0],reply,args,argsLen)){
            reply("Command not found! Try 'help' for more information.\r\n");
        }
    }

    //Display Prefix
    snprintf(OUT,LONG,"%s>",hostname.c_str());
    reply(OUT);

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
void EspToolkit::variableLoad(bool save, bool reset){
    if(!isBegin) isBegin = true;
    AOS_VAR* i{aos_var};
    // Open NVS
    ESP_LOGI("TOOLKIT","Opening Non-Volatile Storage (NVS) handle... ");
    nvs_handle my_handle;
    esp_err_t ret;
    ret = nvs_open(NVS_DEFAULT_PART_NAME, NVS_READWRITE, &my_handle);
    if (ret != ESP_OK) {
        ESP_LOGI("TOOLKIT","Opening Non-Volatile Storage (NVS) handle...");
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
                    size_t required_size;
                    if(nvs_get_str(my_handle, i->name, NULL, &required_size) == ESP_OK && required_size > 0){
                        char* buffer = (char*)malloc(required_size);
                        if(nvs_get_str(my_handle, i->name, buffer, &required_size) == ESP_OK){
                            *(std::string*)(i->value) = buffer;
                        };
                        delete[] buffer;
                    }
                }
            }
        i = i->aos_var;
    };
    if(save) nvs_commit(my_handle);
    nvs_close(my_handle);
};

void EspToolkit::variablesAddDefault(){
    variableAdd("hostname", hostname,   "🖥  System Hostname");
    variableAdd("password", password,   "🖥  System Password");
    variableAdd("locked",   locked,     "🖥  Locked");
    variableAdd("cpuFreq",  cpuFreq,    "🖥  CPU Speed: 0=80MHz | 1=160MHz | 2=240MHz");
    variableAdd("loglevel", logLevel,   "🖥  Loglevel: 0=Error | 1=Warning | 2=info | 3=Debug | 4=Verbose");
};

void EspToolkit::commandAddDefault(){

    #ifndef TK_DISABLE_GPIO_COMMAND
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
                EspToolkitInstance->commandMan("gpio",reply);
                return;
            }
        },NULL,"🖥  gpio [pin] [0|1]");
    #endif

    commandAdd("help",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char* filter = parCnt == 2 ? param[1] : NULL;
        reply("\r\nCommands:\r\n");
        AOS_CMD* i{EspToolkitInstance->aos_cmd};
        while(i != nullptr){
            if(!i->hidden && (!filter || strstr(i->name, filter))){
                snprintf(OUT,LONG, "%-30s %s\r\n",i->name,i->description);reply(OUT);
            }
            i = i->aos_cmd;
        };
    },NULL,"",true);

    commandAdd("get",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        char* filter = parCnt == 2 ? param[1] : NULL;
        reply("Variables:\r\n");
        AOS_VAR* i{EspToolkitInstance->aos_var};
        while(i != nullptr){
            if(!i->hidden && (!filter || strstr(i->name, filter))){ 
                if(i->aos_dt==AOS_DT_BOOL)   snprintf(OUT,LONG,"%-30s : %-40s\t%s %s\r\n", i->name,*(bool*)(i->value) ? "true" : "false",i->description,(i->protect ? "(Protected)":""));
                if(i->aos_dt==AOS_DT_INT)    snprintf(OUT,LONG,"%-30s : %-40d\t%s %s\r\n", i->name,*(int*)(i->value),i->description,(i->protect ? "(Protected)":""));
                if(i->aos_dt==AOS_DT_DOUBLE) snprintf(OUT,LONG,"%-30s : %-40f\t%s %s\r\n", i->name,*(double*)(i->value),i->description,(i->protect ? "(Protected)":""));
                if(i->aos_dt==AOS_DT_STRING) {
                    size_t length  = (*(std::string*)(i->value)).length();
                    bool   newLine = (*(std::string*)(i->value)).find('\n') != std::string::npos;
                    char length_str[32]{0};
                    snprintf(length_str,32,"[ Length: %zu Bytes ]",length);
                    snprintf(OUT,LONG,"%-30s : %-40s\t%s %s\r\n", i->name,(length > 32 || newLine) ? length_str : (*(std::string*)(i->value)).c_str(),i->description,(i->protect ? "(Protected)":""));
                    if(filter && strcmp(filter, i->name) == 0){
                        reply(OUT);
                        reply((*(std::string*)(i->value)).c_str());
                        reply("\r\n");
                        memset(OUT, 0, LONG);
                    }
                }
                reply(OUT);
            }
            i = i->aos_var;
        };
    },NULL,"🖥  get [filter]");

    commandAdd("set",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt < 2 || parCnt > 3){
            reply("Invalid parameter!\r\n");
            EspToolkitInstance->commandMan("set",reply);
            return;
        }
        char* name  = param[1];
        char* value = parCnt == 3 ? param[2] : NULL;
        AOS_VAR* i{EspToolkitInstance->aos_var};
        while(i != nullptr){
            if(!strcmp(i->name,name)){
                if(i->protect){
                    reply("Parameter is protected!\r\n");
                    return;
                }
                if(i->aos_dt==AOS_DT_BOOL)      *(bool*)(i->value)        = !value ? false   : (!strcmp(value,"1") || !strcmp(value,"true")) ? true : false;
                if(i->aos_dt==AOS_DT_INT)       *(int*)(i->value)         = !value ? 0       : atoi(value);
                if(i->aos_dt==AOS_DT_DOUBLE)    *(double*)(i->value)      = !value ? 0.0     : atof(value); 
                if(i->aos_dt==AOS_DT_STRING)    *(std::string*)(i->value) = !value ? "" : value;
                EspToolkitInstance->variableLoad(true);
                reply("ok\r\n");
                return;
            }
            i = i->aos_var;
        };
        reply("Parameter not found!\r\n");
    },NULL,"🖥  set [par] [val]");

    commandAdd("clear",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("\033[2J\033[11H\r\n");
    },NULL,"",true);

    commandAdd("lock",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("SYSTEM LOCKED!\r\n");
        EspToolkitInstance->locked=true;
        EspToolkitInstance->variableLoad(true);
    },NULL,"🖥  lock current shell");

    commandAdd("reboot",[](void*, void (*reply)(const char*), char** param,uint8_t parCnt){
        esp_restart();
    },NULL,"🖥  Reboot");

    commandAdd("reset",[](void*c , void (*reply)(const char*), char** param,uint8_t parCnt){
        EspToolkitInstance->variableLoad(false,true);
        esp_restart();
    },NULL,"🖥  Reset Values stored in NVS to defaults");

    commandAdd("status",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        reply("🖥  System:\r\n");
        snprintf(OUT,LONG,"%-30s : %s\r\n","COMPILED",__DATE__ " " __TIME__);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","IDF Version",esp_get_idf_version());reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","TOOLKIT Version",TOOLKITVERSION);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","APP Version",EspToolkitInstance->appVersion.c_str());reply(OUT);
        /*esp_pm_config_esp32_t pm_config;
        esp_pm_get_configuration(&pm_config);
        snprintf(OUT,LONG,"%-30s : %d MHz\r\n","CPU Max Freq",pm_config.max_freq_mhz);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %d MHz\r\n","CPU Min Freq",pm_config.min_freq_mhz);reply(OUT);
        snprintf(OUT,LONG,"%-30s : %s\r\n","CPU light sleep enabled",pm_config.light_sleep_enable ? "true" : "false");reply(OUT);*/
        snprintf(OUT,LONG,"%-30s : %d MHz\r\n","CPU Freq",getCpuFrequencyMhz());reply(OUT);
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
        EspToolkitInstance->timer.printTasks(reply);
    },NULL,"🖥  Print active syncTimers");
};

// Globals
EspToolkit*  EspToolkitInstance{nullptr};
char         OUT[LONG]{0};

double mapVal(double x, int in_min, int in_max, int out_min, int out_max)
{
    double r = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if(r > out_max) r = out_max;
    if(r < out_min) r = out_min;
    return r;
}

std::vector<std::string>     split(std::string s, std::string delimiter) {
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