@echo off

call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -startdir=none -arch=x64 -host_arch=x64

@REM if exist lib rmdir /s /q lib
@REM mkdir lib
@REM pushd lib

set arg1=%1

if not exist lib mkdir lib
pushd lib

set CommonCompilerOptions=/nologo /GR- /FC /GS- /Gs9999999 /Felib.dll

set CompilerOptionsDev=/Zi /Od /LDd
set CompilerOptionsProd=/O2 /LDd

set LinkerOptions=/nodefaultlib /STACK:0x100000,0x100000 /incremental:no -EXPORT:RenderApp -EXPORT:OnLibEvent
set Libs=user32.lib kernel32.lib gdi32.lib opengl32.lib dwmapi.lib winmm.lib shell32.lib ole32.lib uuid.lib


IF "%arg1%" == "p" (
    echo Production build
    cl %CommonCompilerOptions% %CompilerOptionsProd% ../main_lib.c /link %LinkerOptions% %Libs% 
)

IF NOT "%arg1%" == "p" (
    echo Development build
    cl %CommonCompilerOptions% %CompilerOptionsDev% ../main_lib.c /link %LinkerOptions% %Libs% 
)


popd