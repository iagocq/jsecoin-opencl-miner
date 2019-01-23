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
#include <inttypes.h>
#include <sha256.cl.h>
#include <sha256.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define checkError(error) _checkError(__LINE__, error)
#define plural(number) (number > 1 ? "s" : "")

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

int main(int argc, char *argv[]) {
    int nworkers = 0;
    cl_int error;

    cl_uint platformIdCount = 0;
    clGetPlatformIDs(0, NULL, &platformIdCount);

    if (platformIdCount == 0) {
        fprintf(stderr, "No OpenCL platforms found\n");
        return EXIT_FAILURE;
    } else {
        printf("Found %d platform%s\n", platformIdCount, plural(platformIdCount));
    }

    cl_platform_id *platformIds =
        (cl_platform_id *) alloca(platformIdCount * sizeof(cl_platform_id));
    clGetPlatformIDs(platformIdCount, platformIds, NULL);

    for (cl_uint i = 0; i < platformIdCount; i++) {
        printf("%d:\t%s\n", i, getPlatformName(platformIds[i]));
    }
    int platformId;
    printf("Select platform index to use: ");
    scanf("%d", &platformId);

    cl_uint deviceIdCount = 0;
    clGetDeviceIDs(platformIds[platformId], CL_DEVICE_TYPE_ALL, 0, NULL, &deviceIdCount);

    if (deviceIdCount == 0) {
        fprintf(stderr, "No OpenCL devices found\n");
        return EXIT_FAILURE;
    } else {
        printf("Found %d device%s on platform %d\n", deviceIdCount, plural(deviceIdCount), 0);
    }

    cl_device_id *deviceIds = (cl_device_id *) alloca(deviceIdCount * sizeof(cl_device_id));
    clGetDeviceIDs(platformIds[platformId], CL_DEVICE_TYPE_ALL, deviceIdCount, deviceIds, NULL);

    for (cl_uint i = 0; i < deviceIdCount; i++) {
        printf("%d:\t%s\n", i, getDeviceName(deviceIds[i]));
    }

    int deviceId;
    printf("Select device index to use: ");
    scanf("%d", &deviceId);

    size_t maxItemSizes[3];

    clGetDeviceInfo(deviceIds[deviceId], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(maxItemSizes),
                    maxItemSizes, NULL);
    nworkers = maxItemSizes[0];
    printf("Max dimensions: [%u, %u, %u]\n", maxItemSizes[0], maxItemSizes[1], maxItemSizes[2]);

    cl_context_properties contextProperties[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties) platformIds[platformId], 0, 0};

    cl_context context =
        clCreateContext(contextProperties, deviceIdCount, deviceIds, NULL, NULL, &error);
    checkError(error);

    size_t sourceLen = 0;
    char *source = sha256CLSource;

    cl_program program = createProgram(source, sourceLen, context, &error);
    checkError(error);

    error = clBuildProgram(program, deviceIdCount, deviceIds, NULL, NULL, NULL);
    if (error == CL_BUILD_PROGRAM_FAILURE) {
        size_t logSize;
        clGetProgramBuildInfo(program, deviceIds[deviceId], CL_PROGRAM_BUILD_LOG, 0, NULL,
                              &logSize);
        char *buildLog = (char *) malloc(logSize);
        clGetProgramBuildInfo(program, deviceIds[deviceId], CL_PROGRAM_BUILD_LOG, logSize, buildLog,
                              NULL);
        fprintf(stderr, "Build error!\n%s\n", buildLog);
        return EXIT_FAILURE;
    }
    checkError(error);

    cl_kernel kernel = clCreateKernel(program, "sha256", &error);
    checkError(error);

    size_t globalWorkSize[3];

    for (int i = 0; i < 3; i++) {
        printf("Insert the value for dimension %d of the work size: ", i + 1);
        scanf("%u", &globalWorkSize[i]);
    }

    char prehash[65] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    uint32_t state[8];

    sha256_init(state);
    sha256_round((uint8_t *) prehash, state);

    uint32_t nitems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];

    cl_mem prehashBuffer =
        clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 64, state, &error);
    checkError(error);
    cl_mem resultBuffer =
        clCreateBuffer(context, CL_MEM_WRITE_ONLY, nitems * sizeof(uint32_t), NULL, &error);
    checkError(error);

    cl_command_queue queue = clCreateCommandQueue(context, deviceIds[deviceId], 0, &error);
    checkError(error);

    cl_uint startNonce = 0;
    cl_uint difficultyMask = 0xFFFF0000;
    clSetKernelArg(kernel, 0, sizeof(cl_mem), &prehashBuffer);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &resultBuffer);
    clSetKernelArg(kernel, 2, sizeof(cl_uint), &startNonce);
    clSetKernelArg(kernel, 3, sizeof(cl_uint), &difficultyMask);

#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER t1, t2;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t1);
#else
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);
#endif

    error = clEnqueueNDRangeKernel(queue, kernel, 3, NULL, globalWorkSize, NULL, 0, NULL, NULL);
    checkError(error);

    error = clFinish(queue);
    checkError(error);

    uint32_t *result = (uint32_t *) malloc(nitems * sizeof(uint32_t));
    error = clEnqueueReadBuffer(queue, resultBuffer, CL_TRUE, 0, nitems * 4, result, 0, NULL, NULL);
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

    clReleaseCommandQueue(queue);

    clReleaseMemObject(prehashBuffer);
    clReleaseMemObject(resultBuffer);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    clReleaseContext(context);

    return 0;
}
