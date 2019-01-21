# JSECoin OpenCL Hasher
A JSECoin OpenCL-based SHA-256 hasher for better hashrates.

## About
This project is incomplete to be actually useful for the time being. And it needs a cleanup.
In the future, I plan to make it serve hashes and also be feeded prehashes to work on so some application can use it.

## Building
Some form of OpenCL ICD must be installed to be linked against the program. You can use one from your CPU or GPU vendor or compile the [Khronos Group's OpenCL ICD Loader](https://github.com/KhronosGroup/OpenCL-ICD-Loader).

Place the OpenCL headers on the `include` folder and follow the instructions for each platform.
CMake and Make must be installed.
The built files will be at `build/bin` if the build was successful.

### Windows
Make sure MinGW is installed and run:
```bat
build_win.bat
```

### Linux
Run:
```sh
make
```