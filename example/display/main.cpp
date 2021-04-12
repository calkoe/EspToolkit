#include <EspToolkit.h>
#include <Network.h>
#include <Mqtt.h>

#include "include/ssd1306/ssd1306.h"

EspToolkit  tk;
Network     net(&tk);
Mqtt        mqtt(&tk);
SSD1306_t   dev;
int         center, top, bottom;
char        lineChar[20];
void        initDisplay();

void setup()
{
    vTaskPrioritySet(NULL, 3);

    //Setup EspToolkit
    tk.begin();
    net.commit();

    // Init Display
    initDisplay();

    // Send and Receive MQTT Messages
    tk.events.on(0,"MQTT_EVENT_CONNECTED",[](void* ctx, void* arg){
        mqtt.subscribe((char*)"demo1");
        mqtt.publish((char*)"demo2",(char*)"HALLO 1234",0,0);
    });

    tk.events.on(0,"demo1",[](void* ctx, void* arg){
        std::cout << "[demo1] Received Message: " << (char*)arg << std::endl;
        mqtt.publish((char*)"demo3",(char*)arg);
    });

}

void loop()
{
    tk.loop();
    vTaskDelay(5);
}

void initDisplay(){

    // Init Display and Listen to Wifi and Mqtt Events
    i2c_master_init(&dev, 21, 22, -1);
    ssd1306_init(&dev, 128, 32);
    ssd1306_clear_screen(&dev, false);
	ssd1306_contrast(&dev, 0xff);
	ssd1306_clear_line(&dev, 0, true);
	ssd1306_display_text(&dev, 0, "EspToolkit 0.1", 14, true);
    tk.events.on(0,"SYSTEM_EVENT_STA_DISCONNECTED",[](void*, void*){
        ssd1306_clear_line(&dev, 1, true);
	    ssd1306_display_text(&dev, 1, (char*)"WIFI Disconnected", 17, true);
    });
    tk.events.on(0,"SYSTEM_EVENT_STA_START",[](void*, void*){
        ssd1306_clear_line(&dev, 1, true);
	    ssd1306_display_text(&dev, 1, (char*)"WIFI Connecting", 15, true);
    });
    tk.events.on(0,"SYSTEM_EVENT_STA_CONNECTED",[](void*, void*){
        ssd1306_clear_line(&dev, 1, false);
	    ssd1306_display_text(&dev, 1, (char*)"WIFI Connected", 14, false);
    });
    tk.events.on(0,"MQTT_EVENT_DISCONNECTED",[](void*, void*){
        ssd1306_clear_line(&dev, 2, true);
	    ssd1306_display_text(&dev, 2, (char*)"MQTT Disconnected", 17, true);
    });
    tk.events.on(0,"MQTT_EVENT_BEFORE_CONNECT",[](void*, void*){
        ssd1306_clear_line(&dev, 2, true);
	    ssd1306_display_text(&dev, 2, (char*)"MQTT Connecting", 15, true);
    });
    tk.events.on(0,"MQTT_EVENT_CONNECTED",[](void*, void*){
        ssd1306_clear_line(&dev, 2, false);
	    ssd1306_display_text(&dev, 2, (char*)"MQTT Connected", 14, false);
    });
    tk.events.on(0,"SYSTEM_EVENT_STA_LOST_IP",[](void*, void*){
        ssd1306_clear_line(&dev, 3, true);
	    ssd1306_display_text(&dev, 3, (char*)"IP Lost", 7, true);
    });
    tk.events.on(0,"SYSTEM_EVENT_STA_GOT_IP",[](void*, void*){
        ssd1306_clear_line(&dev, 3, false);
        char buf[16]{0};net.getStaIpStr(buf);
	    ssd1306_display_text(&dev, 3, buf, 15, false);     
    });
}