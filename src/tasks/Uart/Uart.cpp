#include "Uart.h"

Uart* Uart::_this{nullptr};

Uart::Uart(){
    if(_this) return;
    _this = this;

    //fflush(stdout);
    //fsync(fileno(stdout));

    /* Disable buffering on stdin */
    //setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    //esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    //esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /*const uart_config_t uart_config = {
        .baud_rate  = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
                .source_clk = UART_SCLK_REF_TICK,
        #else
                .source_clk = UART_SCLK_XTAL,
        #endif
    };*/
    
    /* Install UART driver for interrupt-driven reads and writes */
    //ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,256, 0, 0, NULL, 0) );
    //ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );

    /* Tell VFS to use UART driver */
    //esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    /* Config line In */
    lineIn.setOnEcho([](char* str, void* ctx){
        std::cout << str;
    },NULL);

    lineIn.setOnLine([](char* str, void* ctx){
        EspToolkitInstance->commandParseAndCall(str,print);
    },NULL);

    // TASK
    xTaskCreate([](void* ctx){
        uint8_t c;

        // Say Hello!
        EspToolkitInstance->commandParseAndCall((char*)"help",print);

        while(true){
            while((c = (uint8_t)fgetc(stdin)) != 0xFF) _this->lineIn.in(c);
            vTaskDelay(10);
        }
         
    }, "uart", 4096, NULL, 1, NULL);
    
};

void Uart::print(const char* text){
    std::cout << text;
};