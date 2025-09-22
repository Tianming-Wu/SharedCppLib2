/*
    Pipeline utils in SharedCppLib2, 2025.05.13, version 0.1
    Currently, only linux supported.
*/
#pragma once

#include <string>
#include <cstdio>
#include <memory>
#include <thread>
#include <atomic>
#include <exception>
#include "stringlist.hpp"

namespace pipeline {

class splitter
{
public:
    splitter(const char* command)
        : pipe(popen(command.c_str(), "r"), pclose)
    {
        if (!pipe) {
            throw std::runtime_error("Failed to start pipeline");
        }
    }
    
    /// @brief getline in a pipeline, recognizing '\r' and'\n' as line splitter.
    /// use this like while(getline(..., succ)) { if(succ) { ... } }
    /// @return result of fgets, put this in while()
    /// @param success whether there is a new line
    bool getline(std::string& get, bool &success) {
        success = false;
    
        char buffer[256];
        if( ! fgets(buffer, sizeof(buffer), pipe.get()) ) return false;
        std::string strbuf(buffer);
        if(strbuf.empty()) return true;
        
        size_t tpos;
        if( ((tpos = strbuf.find('\n')) != std::string::npos) || ((tpos = strbuf.find('\r')) != std::string::npos) ) {
            get = m_rest + strbuf.substr(0, tpos);
            m_rest = strbuf.substr(tpos+1);
        } else {
            m_rest += strbuf;
            return true;
        }
        
        success = true;
        return true;
    }
    

private:
    // size_t m_bufferSize;
    std::strling m_rest;
    std::unique_ptr<FILE, decltype(&pclose)> pipe;
};


}
