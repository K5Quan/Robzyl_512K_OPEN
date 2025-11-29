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
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make clean && make -s ENABLE_FR_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.fr            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.pl            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.ro            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.ko            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.cz            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.tu            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1   ENABLE_EEPROM_512K=1                                               TARGET=robzyl.512k.ru            && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_FR_BAND=1                                                                      TARGET=robzyl.fr                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1                                                                      TARGET=robzyl.pl                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1                                                                      TARGET=robzyl.ro                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1                                                                      TARGET=robzyl.ko                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1                                                                      TARGET=robzyl.cz                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1                                                                      TARGET=robzyl.tu                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1                                                                      TARGET=robzyl.ru                 && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_FR_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.fr && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.pl && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ro && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ko && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.cz && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.tu && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1   ENABLE_EEPROM_512K=1 SCANLIST_SHOW_DETAIL=0  ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ru && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_FR_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.fr      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.pl      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ro      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ko      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.cz      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.tu      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1                                                ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ru      && cp *packed.bin compiled-firmware/"


time /t
pause