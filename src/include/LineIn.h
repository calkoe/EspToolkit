#pragma once

#include <cstring>
#include <stdio.h>
#include <iostream>

/**
 * @brief Simple CLI
 * @author Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date 4.2021
*/

#include <string>
class LineIn{

    private:

        unsigned char   IOC{0};

        std::string     _buffer[2];
        bool            _marks[2]{false};

        bool            esc(char);
        void            (*onEcho)(char*,void*);
        void*           onEchoArg;
        void            (*onLine)(char*,void*);
        void*           onLineArg;
        enum ESC_T      { ESC_STATE_NONE, ESC_STATE_START, ESC_STATE_CODE} inEsc {ESC_STATE_NONE};

    public:

        LineIn();

        void    setOnEcho(void (*)(char*,void*), void*);
        void    setOnLine(void (*)(char*,void*), void*);
        void    in(char);
        void    in(char*);

        void    printClear();

};