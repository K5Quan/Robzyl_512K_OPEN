@echo off
cls
del .\compiled-firmware\*.bin
docker build -t uvk5 .
REM docker run --rm -v %CD%\cmpiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make clean && make -s ENABLE_FR_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.fr && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.pl && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ro && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ko && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.cz && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.tu && cp *packed.bin compiled-firmware/"
REM docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1   ENABLE_EEPROM_512K=1   ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.512k.ru && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_FR_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.fr      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_PL_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.pl      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RO_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ro      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_KO_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ko      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_CZ_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.cz      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_TU_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.tu      && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s               ENABLE_RU_BAND=1                          ENABLE_SCREENSHOT=1   TARGET=robzyl.screenshot.ru      && cp *packed.bin compiled-firmware/"

time /t
pause