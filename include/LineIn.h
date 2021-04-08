#pragma once

#include <cstring>
#include <stdio.h>

/**
 * @brief Simple CLI
 * @author Calvin Köcher | calvin.koecher@alumni.fh-aachen.de
 * @date 4.2021
*/
template <unsigned BUFFER_SIZE>
class LineIn{

    private:

        unsigned char   IOC;
        unsigned        IOP[2];
        char            IO[2][BUFFER_SIZE];
        char            OUT[BUFFER_SIZE];
        bool            esc(char);
        void            (*onEcho)(char*,void*);
        void*           onEchoArg;
        void            (*onLine)(char*,void*);
        void*           onLineArg;
        enum ESC_T      { ESC_STATE_NONE, ESC_STATE_START, ESC_STATE_CODE} inEsc {ESC_STATE_NONE};
        char            lc;
        bool            locked{false};
        char*           password = "tk";

    public:


        LineIn();

        void    setOnEcho(void (*)(char*,void*), void*);
        void    setOnLine(void (*)(char*,void*), void*);
        void    in(char);
        void    in(char*);

        void    clear();
        void    printClear();

        void    lock();
        void    unlock();
        void    setPassword(char* password);
};

template <unsigned BUFFER_SIZE>
LineIn<BUFFER_SIZE>::LineIn(){
    clear();
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::setOnEcho(void (*onEcho)(char*,void*), void* onEchoArg){
    this->onEcho    = onEcho;
    this->onEchoArg = onEchoArg;
    onEcho((char*)"\033[2J\033[11H",onEchoArg);
    printClear();
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::setOnLine(void (*onLine)(char*,void*), void* onLineArg){
    this->onLine    = onLine;
    this->onLineArg = onLineArg;
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::in(char* ca){
    for (char c = *ca; c; c=*++ca) in(c);
};

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::in(char c){
    //std::cout << (int)c << std::endl;
    //return;
    if(esc(c)) return;                                             //Filter ESC 
    if(c != 13 && c != 10 && c != 8 && c != 127){                  //No Echo on CR or NL or Backspace or DEL
        char ca[]{c,0};
        if(!locked) onEcho(ca,onEchoArg);
    }                  
    if(c == 8 || c == 127){                                        //BACKSPACE or DEL
        if(IOP[IOC] > 0){
            IO[IOC][--IOP[IOC]] = 0;
            if(!locked) onEcho((char*)"\b \b",onEchoArg);
        }
    }else if(c != 13 && c != 10 && IOP[IOC] < BUFFER_SIZE-1){     //CR NL
        IO[IOC][IOP[IOC]++] = c;                    
    }else if((lc != 13 && lc != 10) && c == 10){                  //CR NL 
        onEcho((char*)"\r\n",onEchoArg);     
        if(!std::strcmp(IO[IOC],password)){
            locked = false;
            printClear();
        }else{
            if(!std::strcmp(IO[IOC],"lock"))  locked = true;
            if(locked){
                printClear();
            }else{
                onLine(IO[IOC],onLineArg);
            }       
            IOC = !IOC;
        }
        clear();
    }
    lc=c;
};

template <unsigned BUFFER_SIZE>
bool LineIn<BUFFER_SIZE>::esc(char c){
    bool ret = false;
    if(c == 27 || c == 255){
        inEsc = ESC_STATE_START;
        ret = true;
    }else if(inEsc == ESC_STATE_START){
        if(c == 91 || c == 251 || c == 253){
            inEsc = ESC_STATE_CODE;
            ret = true;
        }else
            inEsc = ESC_STATE_NONE;
    }else if(inEsc == ESC_STATE_CODE){
        switch(c){
            case 65:  //Cursor Up
                IOC = !IOC;
                printClear();
                onEcho(IO[IOC],onEchoArg);
                lc=0;
                break;
            case 66:  //Cursor Down
                IOC = !IOC;
                printClear();
                onEcho(IO[IOC],onEchoArg);
                lc=0;
                break;
        };
        inEsc = ESC_STATE_NONE;
        ret = true;
    }
    return ret;
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::clear(){
    for(unsigned int i{0};i<BUFFER_SIZE;i++)IO[IOC][i]=0;
    IOP[IOC]=0;
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::printClear(){
    onEcho((char*)"\r",onEchoArg);
    for(unsigned i{0};i<BUFFER_SIZE-1;i++) OUT[i] = 32;
    onEcho(OUT,onEchoArg);
    onEcho((char*)"\r",onEchoArg);
    if(locked){
        onEcho((char*)"##### LOCKED #####\r\nPlease enter Password: ",onEchoArg);  
    }else{
        onEcho((char*)"ESPToolkit:/>",onEchoArg);
    }
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::lock(){
    locked = true;
    printClear();
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::unlock(){
    locked = false;
    printClear();
}

template <unsigned BUFFER_SIZE>
void LineIn<BUFFER_SIZE>::setPassword(char* password){
    this->password = password;
};
