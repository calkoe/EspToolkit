
#include "LineIn.h"

LineIn::LineIn(){}

void LineIn::setOnEcho(void (*onEcho)(char*,void*), void* onEchoArg){
    this->onEcho    = onEcho;
    this->onEchoArg = onEchoArg;
    onEcho((char*)"\033[2J\033[11H",onEchoArg);
}

void LineIn::setOnLine(void (*onLine)(char*,void*), void* onLineArg){
    this->onLine    = onLine;
    this->onLineArg = onLineArg;
}

void LineIn::in(char* ca){
    for (char c = *ca; c; c=*++ca) in(c);
};

void LineIn::in(char c){
    //std::cout << (int)c;
    //return;

    // Escape Commands
    if(esc(c)){
        return;     
    }                            

    // Handle BACKSPACE or DEL
    if(c == 8 || c == 127){     
        if(_buffer[IOC].back() == '"') _marks[IOC] = !_marks[IOC];                 
        if(!_buffer[IOC].empty()){
            onEcho((char*)"\b \b",onEchoArg);
            _buffer[IOC].pop_back();
        }
        return;
    }

    // Toogle Marks
    if(c == '"'){
        _marks[IOC] = !_marks[IOC];
    }

    // Execute
    if(!_marks[IOC] && c == '\n'){     
        onLine((char*)_buffer[IOC].c_str(),onLineArg);
        IOC = !IOC;
        _buffer[IOC].clear();
        return;
    }

    // Echo and Save visible chars
    if((c >= 32 && c <= 126) || c == '\n'){
        char ca[]{c,0};
        onEcho(ca,onEchoArg);
        _buffer[IOC] += c;
        return;
    }

};

bool LineIn::esc(char c){
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
                printClear();
                IOC = !IOC;
                onEcho((char*)_buffer[IOC].c_str(),onEchoArg);
                break;
            case 66:  //Cursor Down
                printClear();
                IOC = !IOC;
                onEcho((char*)_buffer[IOC].c_str(),onEchoArg);
                break;
        };
        inEsc = ESC_STATE_NONE;
        ret = true;
    }
    return ret;
}

void LineIn::printClear(){
    
    std::string out;
    for(unsigned i{0};i<_buffer[IOC].length();i++) out += '\b';
    for(unsigned i{0};i<_buffer[IOC].length();i++) out += ' ';
    for(unsigned i{0};i<_buffer[IOC].length();i++) out += '\b';
    onEcho((char*)out.c_str(),onEchoArg);

}