@echo off
cls
del .\compiled-firmware\*.bin
docker build -t uvk5 .
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make clean && make -s ENABLE_KO_BAND=1 TARGET=robzyl_test_KO && cp *packed.bin compiled-firmware/"
docker run --rm -v %CD%\compiled-firmware:/app/compiled-firmware uvk5 /bin/bash -c "cd /app && make -s ENABLE_KO_BAND=1 ENABLE_EEPROM_512K=1 TARGET=robzyl_test_KO_512k && cp *packed.bin compiled-firmware/"
time /t
pause