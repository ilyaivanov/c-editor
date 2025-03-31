#include <windows.h>

#include <stdint.h>

void RunAndCaptureOutput(char *outBuffer, int32_t *len, BOOL isProd)
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

    u8 *cmd = isProd ? "cmd.exe /C lib.bat p" : "cmd.exe /C lib.bat";

    if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
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

    // DWORD exitCode;
    // GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    // return exitCode;
}
