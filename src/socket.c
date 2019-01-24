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

#include <jseminer/socket.h>

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void zerror(char *msg) {
#ifdef _WIN32
    int err;
    char buf[256];
    buf[0] = '\0';
    err = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);
    if (!*buf)
        sprintf(buf, "%d", err);
    fprintf(stderr, "%s: %s\n", msg, buf);
#else
    perror(msg);
#endif
}

int socketInit(void) {
    setlocale(LC_ALL, "");
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
    return 0;
#endif
}

int socketDeInit(void) {
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

int socketClose(LSOCKET *sock) {
    int status = 0;
    SOCKET s = sock->msocket;
#ifdef _WIN32
    if ((status = shutdown(s, SD_BOTH)) == 0)
        status = closesocket(s);
#else
    if ((status = shutdown(s, SHUT_RDWR)) == 0)
        status = close(s);
#endif
    return status;
}

int socketCreate(LSOCKET *sock, int ver, int type) {
    SOCKET nsock = socket(ver, type, IPPROTO_IP);
    sock->msocket = nsock;
    sock->type = type;
    sock->addr.sin_family = ver;
    return isValidSocket(sock);
}

int isValidSocket(LSOCKET *sock) { return sock->msocket != INVALID_SOCKET; }

int socketConnect(LSOCKET *sock, char *host, unsigned short port) {
    sock->addr.sin_addr.s_addr = inet_addr(host);
    sock->addr.sin_port = htons(port);
    return connect(sock->msocket, (struct sockaddr *) &sock->addr, sizeof(sock->addr));
}

int socketSend(LSOCKET *sock, char *message, int len) { return send(sock->msocket, message, len, 0); }

int socketRecv(LSOCKET *sock, char *buf, int len) { return recv(sock->msocket, buf, len, 0); }

int socketBind(LSOCKET *sock, char *address, unsigned short port) {
    sock->addr.sin_addr.s_addr = inet_addr(address);
    sock->addr.sin_port = htons(port);
    return bind(sock->msocket, (struct sockaddr *) &sock->addr, sizeof(sock->addr));
}

int socketListen(LSOCKET *sock, int backlog) { return listen(sock->msocket, backlog); }

int socketAccept(LSOCKET *sock, LSOCKET *client) {
    int c = sizeof(struct sockaddr_in);
    client->msocket = accept(sock->msocket, (struct sockaddr *) &client->addr, &c);
    client->type = sock->type;
    return isValidSocket(client);
}

void getAddress(LSOCKET *sock, char *buf) { buf = inet_ntoa(sock->addr.sin_addr); }

int getPort(LSOCKET *sock) { return ntohs(sock->addr.sin_port); }
