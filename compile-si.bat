@echo off
cls
if not exist version.txt (
    echo 1 > version.txt
)
set /p VERSION=<version.txt
set /a VERSION=%VERSION%+1
echo %VERSION% > version.txt
set INDEX_FILE=index.h

if exist "%INDEX_FILE%" (
    (
        for /f "delims=" %%A in (%INDEX_FILE%) do (
            echo %%A | findstr /C:"#define APP_VERSION" >nul
            if not errorlevel 1 (
                echo #define APP_VERSION %VERSION%
            ) else (
                echo %%A
            )
        )
    ) > "%INDEX_FILE%.tmp"

    move /Y "%INDEX_FILE%.tmp" "%INDEX_FILE%" >nul
    echo APP_VERSION updated to %VERSION%.
) else (
    echo ERROR : %INDEX_FILE% not found !
)

del .\compiled-firmware\*.bin
docker build -t uvk5 .
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make clean && make -s ENABLE_IN_BAND=1 ENABLE_UART=0 ENABLE_SCANLIST_SHOW_DETAIL=0 ENABLE_EEPROM_512K=0 TARGET=robzyl8k_beta.en.fr && cp *packed.bin compiled-firmware/"
time /t
pause