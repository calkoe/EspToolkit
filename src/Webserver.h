#pragma once

#include <esp_http_server.h>
#include "include/Webserver_files.h"
#include "EspToolkit.h"

/**
 * @brief   Webserver
 * @author  Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date    6.2021
*/

class Webserver{

    private:

        static Webserver* _this;
        EspToolkit* tk;


    public:

        Webserver(EspToolkit*);

        void start();
        httpd_handle_t server{NULL};

};