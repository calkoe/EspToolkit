#include <Webserver.h>

Webserver* Webserver::_this{nullptr};

Webserver::Webserver(EspToolkit* tk):tk{tk}{

}

void Webserver::start(){

    /* Start the file server */
    ESP_ERROR_CHECK(start_file_server("/spiffs"));


/*
    // Server Config 
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn   = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI("WWW", "Starting server on port: '%d'", config.server_port);
    esp_err_t err = httpd_start(&server, &config);
    if(err == ESP_OK) {
        // Set URI handlers
        ESP_LOGI("WWW", "Registering URI handlers");
    
    }else{
        ESP_LOGE("WWW", "Error starting server! %s", esp_err_to_name(err));
    }
*/
}