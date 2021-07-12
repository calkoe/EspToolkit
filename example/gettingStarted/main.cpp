#include <EspToolkit.h>
#include <Network.h>
#include <Mqtt.h>

EspToolkit  tk;
Network     net(&tk);
Mqtt        mqtt(&tk);

bool        demoBool{true};
int         demoInt{1234};
double      demoDouble{1234.1234};
std::string demoString{"DEMO"};

struct myStruct_t{
    char*       ca;
    int         num;
};

void setup()
{
    vTaskPrioritySet(NULL, 3);

    // CONNECT VIA UART TO ACCESS SHELL | BAUD 115200

    // Example: Add Variables accesable via shell
    tk.variableAdd("demo/bool",    demoBool);
    tk.variableAdd("demo/int",     demoInt);
    tk.variableAdd("demo/double",  demoDouble);
    tk.variableAdd("demo/string",  demoString, "..comment..", false, false);

    // Example: Add Commands accesable via shell
    tk.commandAdd("myPublisher",[](void* c, void (*reply)(const char*), char** param,uint8_t parCnt){
        if(parCnt>=3){
            mqtt.publish(param[1],param[2]);
            reply((char*)"📨 Message sent!\r\n");
        };
    },NULL,"📡 [topic] [message] | publish a message to topic",false);

    // Init Toolkit and Load Variables from NVS
    tk.begin();

    // Commit Wifi Settings (Mqtt commit hooks on IP_EVENT_STA_GOT_IP)
    net.commit();

    // Send and Receive MQTT Messages
    tk.events.on(0,"MQTT_EVENT_CONNECTED",[](void* ctx, void* arg){
        mqtt.subscribe((char*)"demo1");
        mqtt.publish((char*)"demo2",(char*)"HALLO 1234",0,0);
    });

    tk.events.on(0,"demo1",[](void* ctx, void* arg){
        std::cout << "[demo1] Received Message: " << (char*)arg << std::endl;
        mqtt.publish((char*)"demo3",(char*)arg);
    });

    // Example: Send and receive custom structs via events as void* 
    tk.events.on(0,"MEINETOPIC",[](void* ctx, void* arg){
        myStruct_t m = *(myStruct_t*)arg;
        std::cout << "ca: "  << m.ca  << std::endl;
        std::cout << "num: " << m.num << std::endl;
    });
    char*       ca  = "Das ist ein char*";
    int         num = 3245427;
    myStruct_t myStruct = {
        ca,
        num
    };
    tk.events.emit("MEINETOPIC",(void*)&myStruct,sizeof(myStruct_t));

    // Listen to GPIO Events
    tk.button.add((gpio_num_t)BOOTBUTTON_PIN,GPIO_FLOATING,100,(char*)"bootbutton100ms");
    tk.events.on(0,"bootbutton100ms",[](void* ctx, void* arg){
        if(!*(bool*)arg){
            mqtt.publish((char*)"button",(char*)"PRESSED",0,0);
        }else{
            mqtt.publish((char*)"button",(char*)"RELEASED",0,0);
        }
    });
    //tk.button.remove((gpio_num_t)BOOTBUTTON_PIN);

}

void loop()
{
    tk.loop();
    vTaskDelay(5);
}
