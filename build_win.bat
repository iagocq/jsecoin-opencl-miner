@echo off
IF NOT EXIST build mkdir build
cd build
cmake .. -G "MinGW Makefiles" && mingw32-make
cd ..
