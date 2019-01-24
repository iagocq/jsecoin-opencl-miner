/*
MIT License

Copyright (c) 2019 iagocq

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _JSEMINER_MINER_H_
#define _JSEMINER_MINER_H_

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>

#define checkError(error) _checkError(__LINE__, error)

typedef struct _CL_MINER {
    cl_uint platformCount, deviceCount;
    cl_platform_id *platforms;
    cl_device_id *devices;
    cl_context context;
    cl_program program;
    cl_kernel kernel;
    cl_command_queue commandQueue;
    size_t maxWorkDimensions[3];
} CL_MINER;

void _checkError(int line, cl_int error);
char *getPlatformName(cl_platform_id platformId);
char *getDeviceName(cl_device_id deviceId);
cl_program createProgram(char *source, size_t len, cl_context context, cl_int *error);
int setupMiner(CL_MINER *miner, cl_platform_id platform, cl_device_id device, char *source, char *kernel);
int getPlatforms(CL_MINER *miner);
int getDevices(CL_MINER *miner, cl_platform_id platform, cl_device_type deviceType);
int getMaxWorkDimensions(CL_MINER *miner, cl_device_id device);
void releaseMiner(CL_MINER *miner);
void initMiner(CL_MINER *miner);

#endif