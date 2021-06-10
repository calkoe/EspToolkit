#include "Telnet.h"

Telnet* Telnet::_this{nullptr};


Telnet::Telnet(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic):events{events},commandTopic{commandTopic}{
    if(_this) return;
    _this = this;

    // EVT_TK_BROADCAST -> SERIAL
    events->on(0,broadcastTopic,[](void* ctx, void* arg){
        print((char*)arg);
    },NULL);

    // TASK
    xTaskCreate([](void* ctx){
        struct sockaddr_in clientAddress;
        struct sockaddr_in serverAddress;

        // Create a socket that we will listen upon.
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            ESP_LOGE("TELNET", "socket: %d %s", sock, strerror(errno));
            vTaskDelete(NULL);
        }

        // Bind our server socket to a port.
        serverAddress.sin_family        = AF_INET;
        serverAddress.sin_addr.s_addr   = htonl(INADDR_ANY);
        serverAddress.sin_port          = htons(23);
        int rc  = bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
        if (rc < 0) {
            ESP_LOGE("TELNET", "bind: %d %s", rc, strerror(errno));
            vTaskDelete(NULL);

        }

        // Flag the socket as listening for new connections.
        rc = listen(sock, 5);
        if (rc < 0) {
            ESP_LOGE("TELNET", "listen: %d %s", rc, strerror(errno));
            vTaskDelete(NULL);
        }

        while (1) {
            // Listen for a new client connection.
            socklen_t clientAddressLength = sizeof(clientAddress);
            _this->clientSock = accept(sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
            if (_this->clientSock < 0) {
                ESP_LOGE("TELNET", "accept: %d %s", _this->clientSock, strerror(errno));
                continue;
            }

            // We now have a new client ... lets say hello!
            simple_cmd_t simple_cmd{
                strdup("help"), 
                print
            };
            _this->events->emit(_this->commandTopic,(void*)&simple_cmd,sizeof(simple_cmd_t));

            // Loop reading data.
            char* readBuffer = (char*)malloc(BUFFER_SIZE);
            bool  first{true};
            while(1) {
                
                memset(readBuffer, '\0', BUFFER_SIZE);
                ssize_t sizeRead = recv(_this->clientSock, readBuffer, BUFFER_SIZE, 0);
                if (sizeRead < 0) {
                    ESP_LOGE("TELNET", "recv: %d %s", sizeRead, strerror(errno));
                    break;
                }
                if (sizeRead == 0) {
                    break;
                }
                if(first){
                    first = false;
                    continue;
                }

                // Process Input
                for (size_t i{0}; i<sizeRead; i++){
                    _this->in(readBuffer[i]);
                }

                // Delay
                vTaskDelay(10);
            }
            free(readBuffer);
            close(_this->clientSock);
            _this->clientSock = -1;
        }


    }, "telnet", 2048, NULL, 0, NULL);
    
}

void Telnet::print(const char* text){
    send(_this->clientSock, text, strlen(text), 0);
} 

void Telnet::in(char c){

    // Toogle Marks
    if(c == '"'){
        _marks = !_marks;
    }

    // Execute
    if(!_marks && c == '\n'){     
        simple_cmd_t simple_cmd{
            strdup(_buffer.c_str()), 
            print
        };
        events->emit(_this->commandTopic,(void*)&simple_cmd,sizeof(simple_cmd_t));
        _buffer.clear();
        return;
    }

    // Echo and Save visible chars
    if((c >= 32 && c <= 126) || c == '\n'){
        _buffer += c;
        return;
    }

}