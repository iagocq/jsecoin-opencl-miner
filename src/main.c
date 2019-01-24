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
#include <jseminer/miner.h>
#include <jseminer/sha256.cl.h>
#include <jseminer/sha256.h>
#include <jseminer/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

int doMineRound(CL_MINER *miner, size_t *workSize, cl_mem resultBuffer, uint8_t *result) {
    cl_uint nitems = workSize[0] * workSize[1] * workSize[2];
    cl_int error = 0;

    error =
        clEnqueueNDRangeKernel(miner->commandQueue, miner->kernel, 3, NULL, workSize, NULL, 0, NULL, NULL);
    fCheckError(error);

    error = clFinish(miner->commandQueue);
    fCheckError(error);

    error = clEnqueueReadBuffer(miner->commandQueue, resultBuffer, CL_TRUE, 0, nitems, result, 0, NULL, NULL);
    checkError(error);

    return 1;
}

int main(int argc, char *argv[]) {
    cl_int error = 0;
    size_t globalWorkSize[3];
    unsigned int deviceIdx, platformIdx;

    LSOCKET *sock = (LSOCKET *) malloc(sizeof(LSOCKET));
    LSOCKET *client = (LSOCKET *) malloc(sizeof(LSOCKET));

    unsigned short bindPort = 9854;
    char *bindIP = "127.0.0.1";
    int c;
    int connected = 0;
    int end = 0;

    cl_ulong startNonce = 0;
    cl_uint difficultyMask = 0xFFFF0000;
    char prehash[64];
    uint32_t hashedPrehash[8];

    CL_MINER miner;

    char netBuf[72];

    struct timeval defaultTimeout = {0, 0};

    if (argc < 6) {
        fprintf(stderr,
                "Usage: %s <platform ID> <device ID> <Work Dim 0> <Work Dim 1> <Work Dim 2> [bind port] "
                "[bind IP]\n",
                argv[0]);
        fprintf(stderr, "Running without any of the <required arguments> will show values to use for them "
                        "and this message\n");
        fprintf(stderr, "Default bind port: %hu\n", bindPort);
        fprintf(stderr, "Default bind IP: %s\n", bindIP);
        fprintf(stderr, "This program will only allow 1 connection at a time\n");
    }

    socketInit();
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
    for (int i = 0; i < 3; i++) {
        if (globalWorkSize[i] > miner.maxWorkDimensions[i]) {
            fprintf(stderr, "Work Dim %d is greater than the maximum for dimension %d, setting to %d", i, i);
            globalWorkSize[i] = miner.maxWorkDimensions[i];
        }
    }

    if (argc > 6) {
        bindPort = (unsigned short) atoi(argv[6]);
    }
    if (argc > 7) {
        bindIP = argv[7];
    }

    if (!setupMiner(&miner, miner.platforms[0], miner.devices[0], sha256CLSource, "sha256")) {
        fprintf(stderr, "Failed to setup miner\n");
        return EXIT_FAILURE;
    }

    uint32_t nitems = globalWorkSize[0] * globalWorkSize[1] * globalWorkSize[2];

    cl_mem resultBuffer =
        clCreateBuffer(miner.context, CL_MEM_WRITE_ONLY, nitems * sizeof(cl_uchar), NULL, &error);

    uint8_t *roundResult = (uint8_t *) malloc(nitems * sizeof(uint8_t));
    clSetKernelArg(miner.kernel, 1, sizeof(cl_mem), &resultBuffer);

    cl_mem prehashBuffer = NULL;

    end = 1;

    if (!socketCreate(sock, AF_INET, SOCK_STREAM))
        zerror("socketCreate Error");
    else if (socketBind(sock, (char *) bindIP, bindPort) < 0)
        zerror("socketBind Error");
    else if (socketListen(sock, 0))
        zerror("socketListen Error");
    else
        end = 0;

    while (!end) {
        do {
            printf("Waiting for connection...\n");
            if (!socketAccept(sock, client)) {
                zerror("Accept Error");
            }
            printf("Accepted Connection\n");

            if ((c = socketRecv(client, netBuf, 76)) < 76) {
                fprintf(stderr, "Wrong packet size (expected 76, but got %d).\n", c);
                socketClose(client);
            } else {
                difficultyMask = ntohl(((uint32_t *) netBuf)[0]);
                startNonce = ((uint64_t *) &netBuf[4])[0];
                startNonce = ntohll(startNonce);
                memcpy(prehash, &netBuf[12], 64);
                connected = 1;

                sha256_init(hashedPrehash);
                sha256_round((uint8_t *) prehash, hashedPrehash);

                if (prehashBuffer != NULL)
                    clReleaseMemObject(prehashBuffer);

                prehashBuffer = clCreateBuffer(miner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 64,
                                               hashedPrehash, &error);

                clSetKernelArg(miner.kernel, 0, sizeof(cl_mem), &prehashBuffer);
                clSetKernelArg(miner.kernel, 3, sizeof(cl_uint), &difficultyMask);
            }
        } while (!connected);
        cl_ulong nonce = startNonce;
        do {
            fd_set sockset;
            FD_ZERO(&sockset);
            FD_SET(client->msocket, &sockset);
            int result = select(client->msocket + 1, &sockset, NULL, NULL, &defaultTimeout);
            if (result == 1) {
                if ((c = socketRecv(client, netBuf, 76)) < 76) {
                    if (c <= 0) {
                        fprintf(stderr, "Closed connection\n");
                        connected = 0;
                    } else {
                        fprintf(stderr, "Wrong packet size (expected 76, but got %d).\n", c);
                    }
                } else {
                    difficultyMask = ntohl(((uint32_t *) netBuf)[0]);
                    startNonce = ((uint64_t *) &netBuf[4])[0];
                    startNonce = ntohll(startNonce);
                    memcpy(prehash, &netBuf[12], 64);
                    nonce = startNonce;

                    sha256_init(hashedPrehash);
                    sha256_round((uint8_t *) prehash, hashedPrehash);

                    if (prehashBuffer != NULL)
                        clReleaseMemObject(prehashBuffer);

                    prehashBuffer = clCreateBuffer(miner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 64,
                                                   hashedPrehash, &error);

                    clSetKernelArg(miner.kernel, 0, sizeof(cl_mem), &prehashBuffer);
                    clSetKernelArg(miner.kernel, 3, sizeof(cl_uint), &difficultyMask);
                }
            } else if (result == 0) {
                clSetKernelArg(miner.kernel, 2, sizeof(cl_ulong), &nonce);
                if (!doMineRound(&miner, globalWorkSize, resultBuffer, roundResult)) {
                    fprintf(stderr, "Mine error!\n");
                    end = 1;
                }
                for (int i = 0; i < nitems; i++) {
                    if (roundResult[i] == 1) {
                        ((uint64_t *) netBuf)[0] = htonll(nonce + i);
                        socketSend(client, prehash, 64);
                        socketSend(client, netBuf, 8);
                    }
                }
                nonce += nitems;
            } else {
                printf("Select error!\n");
                socketClose(client);
                connected = 0;
            }
        } while (connected && !end);
    }

    socketDeInit();

    clReleaseMemObject(resultBuffer);
    releaseMiner(&miner);

    return 0;
}
