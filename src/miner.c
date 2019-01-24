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

#include <jseminer/miner.h>

#define fCheckError(error) _fCheckError(__LINE__, error)
#define _fCheckError(line, error)                                                                            \
    do {                                                                                                     \
        if (error != CL_SUCCESS) {                                                                           \
            fprintf(stderr, "%d: OpenCL call failed with error code %d\n", line, error);                     \
            return 0;                                                                                        \
        }                                                                                                    \
    } while (0)

void _checkError(int line, cl_int error) {
    if (error != CL_SUCCESS) {
        fprintf(stderr, "%d: OpenCL call failed with error code %d\n", line, error);
        exit(EXIT_FAILURE);
    }
}

char *getPlatformName(cl_platform_id platformId) {
    static char platformName[1024];
    size_t size = 0;
    clGetPlatformInfo(platformId, CL_PLATFORM_NAME, 0, NULL, &size);

    if (size > 1024)
        size = 1024;

    clGetPlatformInfo(platformId, CL_PLATFORM_NAME, size, platformName, NULL);

    return platformName;
}

char *getDeviceName(cl_device_id deviceId) {
    static char deviceName[1024];
    size_t size = 0;
    clGetDeviceInfo(deviceId, CL_DEVICE_NAME, 0, NULL, &size);

    if (size > 1024)
        size = 1024;

    clGetDeviceInfo(deviceId, CL_DEVICE_NAME, size, deviceName, NULL);

    return deviceName;
}

cl_program createProgram(char *source, size_t len, cl_context context, cl_int *error) {
    size_t lengths[1] = {len};
    const char *sources[1] = {source};

    cl_program program = clCreateProgramWithSource(context, 1, sources, lengths, error);

    return program;
}

int setupMiner(CL_MINER *miner, cl_platform_id platform, cl_device_id device, char *source, char *kernel) {
    cl_int error;

    cl_context_properties contextProperties[] = {CL_CONTEXT_PLATFORM, (cl_context_properties) platform, 0, 0};

    miner->context =
        clCreateContext(contextProperties, miner->deviceCount, miner->devices, NULL, NULL, &error);
    fCheckError(error);

    miner->program = createProgram(source, 0, miner->context, &error);
    fCheckError(error);

    error = clBuildProgram(miner->program, miner->deviceCount, miner->devices, NULL, NULL, NULL);
    if (error == CL_BUILD_PROGRAM_FAILURE) {
        size_t logSize;
        clGetProgramBuildInfo(miner->program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        char *buildLog = (char *) malloc(logSize);
        clGetProgramBuildInfo(miner->program, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog, NULL);
        fprintf(stderr, "Build error!\n%s\n", buildLog);
        return 0;
    }
    fCheckError(error);

    miner->kernel = clCreateKernel(miner->program, "sha256", &error);
    fCheckError(error);
    miner->commandQueue = clCreateCommandQueue(miner->context, device, 0, &error);
    fCheckError(error);

    return 1;
}

int getPlatforms(CL_MINER *miner) {
    clGetPlatformIDs(0, NULL, &miner->platformCount);

    if (miner->platformCount == 0)
        return 0;

    miner->platforms = (cl_platform_id *) malloc(miner->platformCount * sizeof(cl_platform_id));
    clGetPlatformIDs(miner->platformCount, miner->platforms, NULL);
    return 1;
}

int getDevices(CL_MINER *miner, cl_platform_id platform, cl_device_type deviceType) {
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, NULL, &miner->deviceCount);

    if (miner->deviceCount == 0)
        return 0;

    miner->devices = (cl_device_id *) malloc(miner->deviceCount * sizeof(cl_device_id));
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, miner->deviceCount, miner->devices, NULL);
    return 1;
}

int getMaxWorkDimensions(CL_MINER *miner, cl_device_id device) {
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(miner->maxWorkDimensions),
                    miner->maxWorkDimensions, NULL);
}

void releaseMiner(CL_MINER *miner) {
    if (miner->devices != NULL)
        free(miner->devices);
    if (miner->platforms != NULL)
        free(miner->platforms);

    clReleaseCommandQueue(miner->commandQueue);

    clReleaseKernel(miner->kernel);
    clReleaseProgram(miner->program);

    clReleaseContext(miner->context);
}

void initMiner(CL_MINER *miner) { memset(miner, 0, sizeof(miner)); }
