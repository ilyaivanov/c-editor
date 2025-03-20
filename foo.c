#include <windows.h>

#include <stdint.h>

void RunAndCaptureOutput(char *outBuffer, int32_t *len)
{
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE}; // Inheritable

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        return;
    }

    // Ensure read handle is not inherited
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = {sizeof(STARTUPINFO)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe; // Capture both stdout and stderr
    si.hStdInput = NULL;
    si.wShowWindow = SW_HIDE; // Prevent console window from appearing

    PROCESS_INFORMATION pi;

    if (!CreateProcess(NULL, "cmd.exe /C lib.bat", NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }

    CloseHandle(hWritePipe);

    char buffer[4096];
    DWORD bytesRead;
    *len = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        memmove(outBuffer + *len, buffer, bytesRead);
        *len = *len + bytesRead;
    }

    CloseHandle(hReadPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void RunWithoutOutput()
{
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    // si.hStdOutput = g_hChildStd_OUT_Wr;
    // si.hStdOutput = hWrite;   // Redirect stdout to pipe
    // si.hStdError = hWrite;    // Redirect stderr to pipe
    si.wShowWindow = SW_HIDE; // Prevent console window from appearing

    if (CreateProcess(NULL, "cmd.exe /C lib.bat", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}