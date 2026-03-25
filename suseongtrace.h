#pragma once
#include <string>

#ifndef NDEBUG // 디버그
    #define TRACE(msg) SuseongTrace::print(__FILE__, __LINE__, __func__, msg);
#else // 릴리즈
    #define TRACE(msg)
#endif

class SuseongTrace
{
public:
    static void print(const char* file, int line, const char* func, const std::string& msg);
private:
    SuseongTrace();
    ~SuseongTrace();
};
