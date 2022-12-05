#pragma once
#ifndef _SIGSCAN_H
#define _SIGSCAN_H

extern bool sigValid;

void* sigScan(const char* signature, const char* mask);

#define SIG_SCAN(x, ...) \
    void* x(); \
    void* x##Addr = x(); \
    void* x() \
    { \
        static const char* x##Data[] = { __VA_ARGS__ }; \
        if (!x##Addr) \
        { \
            for (int i = 0; i < _countof(x##Data); i += 2) \
            { \
                x##Addr = sigScan(x##Data[i], x##Data[i + 1]); \
                if (x##Addr) \
                    return x##Addr; \
            } \
            sigValid = false; \
        } \
        return x##Addr; \
    }

#endif // _SIGSCAN_H
