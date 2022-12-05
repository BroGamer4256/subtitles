#include "SigScan.h"
#include <windows.h>
#include <psapi.h>

bool sigValid = true;

MODULEINFO moduleInfo;

const MODULEINFO& getModuleInfo()
{
    if (moduleInfo.SizeOfImage)
        return moduleInfo;

    ZeroMemory(&moduleInfo, sizeof(MODULEINFO));
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &moduleInfo, sizeof(MODULEINFO));

    return moduleInfo;
}

void* sigScan(const char* signature, const char* mask)
{
    const MODULEINFO& info = getModuleInfo();
    const size_t length = strlen(mask);

    for (size_t i = 0; i < info.SizeOfImage; i++)
    {
        char* memory = (char*)info.lpBaseOfDll + i;

        size_t j;
        for (j = 0; j < length; j++)
        {
            if (mask[j] != '?' && signature[j] != memory[j])
                break;
        }

        if (j == length) 
            return memory;
    }

    return nullptr;
}
