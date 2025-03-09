#include <stdio.h>
#include <stdint.h>
#include <windows.h>

typedef uint64_t u64;
typedef uint32_t u32;

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))

inline u64 Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {0};

    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
    if (FindHandle != INVALID_HANDLE_VALUE)
    {
        LastWriteTime = FindData.ftLastWriteTime;
        FindClose(FindHandle);
    }

    return ((u64)LastWriteTime.dwHighDateTime << 32) | ((u64)LastWriteTime.dwLowDateTime);
}

typedef struct
{
    u64 lastWriteTime;
    char *path;
} FileInfo;

FileInfo files[] = {
    {0, ".\\main2.c"},
    {0, ".\\game.c"},
    {0, ".\\main_editor.c"},
};

int main()
{
    for (u32 i = 0; i < ArrayLength(files); i++)
    {
        FileInfo *file = &files[i];
        file->lastWriteTime = Win32GetLastWriteTime(files[i].path);
    }

    HANDLE hThread = 0;
    DWORD threadId = 0;
    STARTUPINFO si = {sizeof(si)};

    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNOACTIVATE;

    PROCESS_INFORMATION pi = {0};

    while (1)
    {
        u32 hasAnythingChanged = 0;

        for (u32 i = 0; i < ArrayLength(files); i++)
        {
            FileInfo *file = &files[i];
            u64 newWriteTime = Win32GetLastWriteTime(files[i].path);
            if (file->lastWriteTime != newWriteTime)
                hasAnythingChanged = 1;
            file->lastWriteTime = newWriteTime;
        }

        if (hasAnythingChanged)
        {
            if (pi.hProcess > 0)
            {
                TerminateProcess(pi.hProcess, 0);
                CloseHandle(pi.hProcess);
                printf("Closing Process\n");
            }

            Sleep(100);

            printf("Running .\\run.bat\n");
            system(".\\run.bat b");
            CreateProcess(NULL, ".\\build\\main.exe", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            printf("New Process Created\n");
        }
        Sleep(30);
    }
}
