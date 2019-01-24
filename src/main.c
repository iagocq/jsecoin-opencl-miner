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

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/cl.h>
#include <getopt.h>
#include <inttypes.h>
#include <jseminer/sha256.cl.h>
#include <jseminer/sha256.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define checkError(error) _checkError(__LINE__, error)
#define fCheckError(error) _fCheckError(__LINE__, error)
#define _fCheckError(line, error)                                                                            \
    do {                                                                                                     \
        if (error != CL_SUCCESS) {                                                                           \
            fprintf(stderr, "%d: OpenCL call failed with error code %d\n", line, error);                     \
            return 0;                                                                                        \
        }                                                                                                    \
    } while (0)

#define plural(number) (number > 1 ? "s" : "")

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

int main(int argc, char *argv[]) {
    int nworkers = 0;
    cl_int error = 0;
    size_t globalWorkSize[3];
    unsigned int deviceIdx, platformIdx;

    CL_MINER miner;
    if (argc < 6) {
        fprintf(stderr, "Usage: %s [platform ID] [device ID] [<Work Dim X> <Work Dim Y> <Work Dim Z>]\n",
                argv[0]);
    }

    initMiner(&miner);

    getPlatforms(&miner);
    if (miner.platformCount == 0) {
        fprintf(stderr, "No OpenCL platforms available\n");
        return EXIT_FAILURE;
    }
    if (argc < 2) {
        printf("Platforms:\n");
        for (cl_uint i = 0; i < miner.platformCount; i++) {
            printf("ID %u:\t%s\n", i, getPlatformName(miner.platforms[i]));
        }
        return 0;
    }
    platformIdx = atoi(argv[1]);
    if (platformIdx > miner.platformCount) {
        fprintf(stderr, "Invalid platform ID\n");
        return 1;
    }

    getDevices(&miner, miner.platforms[0], CL_DEVICE_TYPE_ALL);
    if (miner.deviceCount == 0) {
        fprintf(stderr, "No OpenCL devices available on this platform\n");
        return EXIT_FAILURE;
    }
    if (argc < 3) {
        printf("Devices:\n");
        for (cl_uint i = 0; i < miner.deviceCount; i++) {
            printf("ID %u:\t%s\n", i, getDeviceName(miner.devices[i]));
        }
        return 0;
    }
    deviceIdx = atoi(argv[1]);
    if (deviceIdx > platformIdx) {
        fprintf(stderr, "Invalid device ID\n");
        return 1;
    }

    getMaxWorkDimensions(&miner, miner.devices[0]);
    if (argc < 6) {
        printf("Max Dimensions: [%u, %u, %u]\n", miner.maxWorkDimensions[0], miner.maxWorkDimensions[1],
               miner.maxWorkDimensions[2]);
        return 0;
    }
    globalWorkSize[0] = (size_t) atoi(argv[3]);
    globalWorkSize[1] = (size_t) atoi(argv[4]);
    globalWorkSize[2] = (size_t) atoi(argv[5]);
    if (globalWorkSize[0] > miner.maxWorkDimensions[0]) {
        fprintf(stderr, "Work Dim X is greater than the maximum for the X dimension, setting to %d\n",
                miner.maxWorkDimensions[0]);
        globalWorkSize[0] = miner.maxWorkDimensions[0];
    }
    if (globalWorkSize[1] > miner.maxWorkDimensions[1]) {
        fprintf(stderr, "Work Dim Y is greater than the maximum for the Y dimension, setting to %d\n",
                miner.maxWorkDimensions[1]);
        globalWorkSize[1] = miner.maxWorkDimensions[1];
    }
    if (globalWorkSize[2] > miner.maxWorkDimensions[2]) {
        fprintf(stderr, "Work Dim Z is greater than the maximum for the Z dimension, setting to %d\n",
                miner.maxWorkDimensions[2]);
        globalWorkSize[2] = miner.maxWorkDimensions[2];
    }

    if (!setupMiner(&miner, miner.platforms[0], miner.devices[0], sha256CLSource, "sha256")) {
        fprintf(stderr, "Failed to setup miner\n");
        return EXIT_FAILURE;
    }

    char prehash[65] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    uint32_t state[8];

    sha256_init(state);
    sha256_round((uint8_t *) prehash, state);

    uint32_t nitems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];

    cl_mem prehashBuffer =
        clCreateBuffer(miner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 64, state, &error);
    checkError(error);
    cl_mem resultBuffer =
        clCreateBuffer(miner.context, CL_MEM_WRITE_ONLY, nitems * sizeof(uint32_t), NULL, &error);
    checkError(error);

    cl_uint startNonce = 0;
    cl_uint difficultyMask = 0xFFFF0000;
    clSetKernelArg(miner.kernel, 0, sizeof(cl_mem), &prehashBuffer);
    clSetKernelArg(miner.kernel, 1, sizeof(cl_mem), &resultBuffer);
    clSetKernelArg(miner.kernel, 2, sizeof(cl_uint), &startNonce);
    clSetKernelArg(miner.kernel, 3, sizeof(cl_uint), &difficultyMask);

#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER t1, t2;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t1);
#else
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
#endif

    error = clEnqueueNDRangeKernel(miner.commandQueue, miner.kernel, 3, NULL, globalWorkSize, NULL, 0, NULL,
                                   NULL);
    checkError(error);

    error = clFinish(miner.commandQueue);
    checkError(error);

    uint32_t *result = (uint32_t *) malloc(nitems * sizeof(uint32_t));
    error =
        clEnqueueReadBuffer(miner.commandQueue, resultBuffer, CL_TRUE, 0, nitems * 4, result, 0, NULL, NULL);
    checkError(error);

    double elapsedTime;
#ifdef _WIN32
    QueryPerformanceCounter(&t2);
    elapsedTime = (float) (t2.QuadPart - t1.QuadPart) / (float) frequency.QuadPart;
#else
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
    elapsedTime /= 1000;
#endif

    printf("Took %.3fs\n", elapsedTime);
    printf("%d Hashes\n", nitems);
    printf("%.3f H/s\n", (double) nitems / elapsedTime);

    for (int i = 0; i < nitems; i++) {
        if (result[i] == 1) {
            printf("%d\n", i);
        }
    }

    clReleaseMemObject(prehashBuffer);
    clReleaseMemObject(resultBuffer);
    releaseMiner(&miner);

    return 0;
}
