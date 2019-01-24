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

#ifndef _JSEMINER_SOCKET_H_
#define _JSEMINER_SOCKET_H_

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#undef UNICODE
#else // _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET_LIB
#define INVALID_SOCKET -1
typedef int SOCKET;
#endif // _WIN32

typedef struct LSOCKET {
    SOCKET msocket;
    struct sockaddr_in addr;
    int type;
} LSOCKET;

void zerror(char *msg);
int socketInit(void);
int socketDeInit(void);
int socketCreate(LSOCKET *sock, int ver, int type);
int socketClose(LSOCKET *sock);
int socketSend(LSOCKET *sock, char *message, int);
int socketRecv(LSOCKET *sock, char *buf, int len);
int socketBind(LSOCKET *sock, char *address, unsigned short port);
int socketListen(LSOCKET *sock, int backlog);
int socketAccept(LSOCKET *sock, LSOCKET *client);
int isValidSocket(LSOCKET *sock);

#endif