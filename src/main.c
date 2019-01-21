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
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
#endif
#define checkError(error) _checkError(__LINE__, error)
#define plural(number) (number > 1 ? "s" : "")

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

const unsigned int k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void sha256round(unsigned char *data, unsigned int *state) {
    unsigned int a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];

    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

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
    
    if (size > 1024) size = 1024;
    clGetPlatformInfo(platformId, CL_PLATFORM_NAME, size, platformName, NULL);

    return platformName;
}

char *readFile(char *filename, size_t *size) {
    FILE *f = fopen(filename, "r");

    fseek(f, 0L, SEEK_END);

    *size = (size_t) ftell(f);
    rewind(f);

    char *contents = malloc(*size);
    fread(contents, 1, *size, f);

    return contents;
}

char *getDeviceName(cl_device_id deviceId) {
    static char deviceName[1024];
    size_t size = 0;
    clGetDeviceInfo(deviceId, CL_DEVICE_NAME, 0, NULL, &size);
    
    if (size > 1024) size = 1024;
    clGetDeviceInfo(deviceId, CL_DEVICE_NAME, size, deviceName, NULL);

    return deviceName;
}

cl_program createProgram(char* source, size_t len, cl_context context, cl_int *error) {
    size_t lengths[1] = { len };
    const char *sources[1] = { source };

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

    cl_platform_id *platformIds = alloca(platformIdCount * sizeof(cl_platform_id));
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

    cl_device_id *deviceIds = alloca(deviceIdCount * sizeof(cl_device_id));
    clGetDeviceIDs(platformIds[platformId], CL_DEVICE_TYPE_ALL, deviceIdCount, deviceIds, NULL);

    for (cl_uint i = 0; i < deviceIdCount; i++) {
        printf("%d:\t%s\n", i, getDeviceName(deviceIds[i]));
    }

    int deviceId;
    printf("Select device index to use: ");
    scanf("%d", &deviceId);

    size_t maxItemSizes[3];

    clGetDeviceInfo(deviceIds[deviceId], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(maxItemSizes), maxItemSizes, NULL);
    nworkers = maxItemSizes[0];
    printf("Max dimensions: [%u, %u, %u]\n", maxItemSizes[0], maxItemSizes[1], maxItemSizes[2]);
    cl_context_properties contextProperties[] = {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties) platformIds[platformId],
        0, 0
    };

    cl_context context = clCreateContext(contextProperties, deviceIdCount, deviceIds, NULL, NULL, &error);
    checkError(error);

    size_t sourceLen = 0;
    char *source = readFile("sha256.cl", &sourceLen);

    cl_program program = createProgram(source, sourceLen, context, &error);
    checkError(error);

    error = clBuildProgram(program, deviceIdCount, deviceIds, NULL, NULL, NULL);
    if (error == CL_BUILD_PROGRAM_FAILURE) {
        size_t logSize;
        clGetProgramBuildInfo(program, deviceIds[deviceId], CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        char *buildLog = malloc(logSize);
        clGetProgramBuildInfo(program, deviceIds[deviceId], CL_PROGRAM_BUILD_LOG, logSize, buildLog, NULL);
        fprintf(stderr, "Build error!\n%s\n", buildLog);
        return EXIT_FAILURE;
    }
    checkError(error);

    cl_kernel kernel = clCreateKernel(program, "sha256", &error);
    checkError(error);

    char prehash[65] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    unsigned int state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    sha256round(prehash, state);

    const size_t globalWorkSize[] = { nworkers, nworkers, 10 };
    unsigned int nitems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];
    cl_mem prehashBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 64, state, &error);
    checkError(error);
    cl_mem resultBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, nitems * 4, NULL, &error);
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

    unsigned int *result = malloc(nitems * 4);
    error = clEnqueueReadBuffer(queue, resultBuffer, CL_TRUE, 0, nitems * 4, result, 0, NULL, NULL);
    checkError(error);

    double elapsedTime;
#ifdef _WIN32
    QueryPerformanceCounter(&t2);
    elapsedTime = (float)(t2.QuadPart - t1.QuadPart) / (float)frequency.QuadPart;
#else
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
    elapsedTime /= 1000;
#endif

    printf("Took %.3fs\n", elapsedTime);
    printf("%d Hashes\n", nitems);
    printf("%.3f H/s\n", (float)nitems/(float)elapsedTime);
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