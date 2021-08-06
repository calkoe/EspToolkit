#pragma once

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "tcpip_adapter.h"
#include "string.h"

#include "EspToolkit.h"
#include "include/captiveDns.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX 64

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

/* IS_FILE_EXT */
#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/**
 * @brief   Webserver
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    6.2021
*/

class Webserver{

    private:

        // Webserver
        static Webserver* _this;
        struct file_server_data {
            char base_path[ESP_VFS_PATH_MAX + 1];
            char scratch[SCRATCH_BUFSIZE];
        };
     
        static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename);
        static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize);
        static esp_err_t download_get_handler(httpd_req_t *req);

    public:

        Webserver();

        esp_err_t start();

        httpd_handle_t      server{NULL};
        httpd_config_t      config = HTTPD_DEFAULT_CONFIG();
        file_server_data*   server_data{NULL};

};