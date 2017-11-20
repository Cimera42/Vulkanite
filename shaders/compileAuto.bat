@echo off
setlocal enabledelayedexpansion

for /f %%f in ('dir /b') do (
    set str=%%f
    set sub=!str:~-5!
    if "!sub!"==".vert" (
        @echo on
        %VK_SDK_PATH%\Bin\glslangValidator -V !str! -o !str!.spv
        @echo off
    )
    if "!sub!"==".frag" (
        @echo on
        %VK_SDK_PATH%\Bin\glslangValidator -V !str! -o !str!.spv
        @echo off
    )
)

rem %VK_SDK_PATH%\Bin\glslangValidator -V standard.vert -o standard.vert.spv