// This example shows how to extract stack trace with filename, line of code and function name
// will be usefull if I will call from IDE my project as a dll
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

#include "main_dep.c"

#pragma comment(lib, "dbghelp.lib")

#define MAX_FRAMES 62

void PrintStackTrace(CONTEXT *context)
{
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();
    SymInitialize(hProcess, NULL, TRUE);

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

#ifdef _M_X64 // 64-bit
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrStack.Offset = context->Rsp;

#else // 32-bit
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrStack.Offset = context->Esp;
#endif
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    int i = 0;
    while (StackWalk64(machineType, hProcess, hThread, &stackFrame, context, NULL,
                       SymFunctionTableAccess64, SymGetModuleBase64, NULL))
    {

        DWORD64 address = stackFrame.AddrPC.Offset;
        if (address == 0)
            break;

        // Get function name
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)] = {0};
        SYMBOL_INFO *symbol = (SYMBOL_INFO *)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        if (SymFromAddr(hProcess, address, NULL, symbol))
        {
            printf("[%d] %s - 0x%llX", i, symbol->Name, (unsigned long long)symbol->Address);
        }
        else
        {
            printf("[%d] Unknown Function - 0x%llX", i, (unsigned long long)address);
        }

        // Get file and line number
        IMAGEHLP_LINE64 line;
        DWORD displacement = 0;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        if (SymGetLineFromAddr64(hProcess, address, &displacement, &line))
        {
            printf(" - %s:%d\n", line.FileName, line.LineNumber);
        }
        else
        {
            printf(" - (unknown file)\n");
        }
        i++;
    }

    SymCleanup(hProcess);
}

void CrashFunctionMain()
{
    Foo();
}

int main()
{
    __try
    {
        CrashFunctionMain();
    }
    __except (PrintStackTrace(GetExceptionInformation()->ContextRecord), EXCEPTION_EXECUTE_HANDLER)
    {
        printf("Exception handled!\n");
    }

    return 0;
}
