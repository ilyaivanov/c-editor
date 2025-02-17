@echo off

@REM Yes, this is a whole build system I have
@REM run     - build DEV version and run
@REM run b   - build DEV version
@REM run p   - build PROD version and run
@REM run p b - build PROD version


set arg1=%1
set arg2=%2

if exist build rmdir /s /q build
mkdir build
pushd build

set CommonCompilerOptions=/nologo /GR- /FC /GS- /Gs9999999

set CompilerOptionsDev=/Zi /Od

set CompilerOptionsProd=/O2 

set LinkerOptions=/nodefaultlib /subsystem:windows /STACK:0x100000,0x100000 /incremental:no
set Libs=user32.lib kernel32.lib gdi32.lib opengl32.lib dwmapi.lib winmm.lib shell32.lib ole32.lib uuid.lib

IF "%arg1%" == "p" (
    echo Production build
    cl %CommonCompilerOptions% %CompilerOptionsProd% ../main.c /link %LinkerOptions% %Libs% 
)

IF NOT "%arg1%" == "p" (
    echo Development build
    cl %CommonCompilerOptions% %CompilerOptionsDev% ../main.c /link %LinkerOptions% %Libs% 
)

IF NOT "%arg1%" == "b" IF NOT "%arg2%" == "b" (
    echo Running...
    call .\main.exe
)

popd