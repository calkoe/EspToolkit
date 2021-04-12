#if defined ESP32

#include "Telnet.h"

Telnet::Telnet(PostOffice<std::string>* events, char* commandTopic, char* broadcastTopic):events{events},commandTopic{commandTopic}{

    // TASK
    xTaskCreate([](void* ctx){
        Telnet* _this = (Telnet*) ctx;
        struct sockaddr_in clientAddress;
        struct sockaddr_in serverAddress;

        // Create a socket that we will listen upon.
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            ESP_LOGE(TAG, "socket: %d %s", sock, strerror(errno));
            vTaskDelete(NULL);
        }

        // Bind our server socket to a port.
        serverAddress.sin_family        = AF_INET;
        serverAddress.sin_addr.s_addr   = htonl(INADDR_ANY);
        serverAddress.sin_port          = htons(23);
        int rc  = bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
        if (rc < 0) {
            ESP_LOGE(TAG, "bind: %d %s", rc, strerror(errno));
            vTaskDelete(NULL);

        }

        // Flag the socket as listening for new connections.
        rc = listen(sock, 5);
        if (rc < 0) {
            ESP_LOGE(TAG, "listen: %d %s", rc, strerror(errno));
            vTaskDelete(NULL);
        }


        while (1) {
            // Listen for a new client connection.
            socklen_t clientAddressLength = sizeof(clientAddress);
            int clientSock = accept(sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
            if (clientSock < 0) {
                ESP_LOGE(TAG, "accept: %d %s", clientSock, strerror(errno));
                vTaskDelete(NULL);
            }

            // We now have a new client ...
            int total =	10*1024;
            int sizeUsed = 0;
            char *data = (char*) malloc(total);

            // Loop reading data.
            while(1) {
                ssize_t sizeRead = recv(clientSock, data + sizeUsed, total-sizeUsed, 0);
                if (sizeRead < 0) {
                    ESP_LOGE(TAG, "recv: %d %s", sizeRead, strerror(errno));
                    vTaskDelete(NULL);
                }
                if (sizeRead == 0) {
                    break;
                }
                sizeUsed += sizeRead;
            }

            // Finished reading data.
            ESP_LOGD(TAG, "Data read (size: %d) was: %.*s", sizeUsed, sizeUsed, data);
            free(data);
            close(clientSock);
        }



    }, "telnet", 2048, this, 1, NULL);
    

    
}

#endif