#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include "config.h"

/*
 * This is a very simple example client socket connection.
 * This implementation is limited to Linux (POSIX) sockets only.
 * A Microsoft Windows implementation is not supplied in this example.
 */

int SocketConnect(const char *cpIpAddress, int iPort) {

    unsigned char ucBuffer[sizeof(struct in6_addr)];

    if(inet_pton(AF_INET, cpIpAddress, ucBuffer) <= 0) {
        printf("IP address %s cannot be converted.\n", cpIpAddress);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(iPort);
    server_addr.sin_addr = *((struct in_addr *) ucBuffer);

    int iSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(iSocket < 0) {
        printf("Cannot create socket. Error %i errno %i.\n", iSocket, errno);
        return iSocket;
    }

    // Receive timeout (configurable via timeout_seconds, default 10)
    struct timeval tv;
    tv.tv_sec = e3dc_config.timeout_seconds;
    tv.tv_usec = 0;
    if (setsockopt(iSocket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv, sizeof(struct timeval)) < 0) {
        printf("Warning: Failed to set receive timeout. errno %i\n", errno);
    }
    // send time out should never occur on normal OS configurations but just in case set the timeout to half of receive timeout
    tv.tv_sec = e3dc_config.timeout_seconds / 2;
    tv.tv_usec = 0;
    if (setsockopt(iSocket, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *) &tv, sizeof(struct timeval)) < 0) {
        printf("Warning: Failed to set send timeout. errno %i\n", errno);
    }

    int enable = 1;
    setsockopt(iSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &enable, sizeof(enable));


    // wait 3 seconds for connection to get ready
    int iRetries = 3;
    if(connect(iSocket, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) < 0) {
        printf("Cannot connect to server. errno %i.\n", errno);
        close(iSocket);
        return -1;
    }

    return iSocket;
}

void SocketClose(int iSocket)
{
    // sanity check
    if(iSocket >= 0) {
        shutdown(iSocket, SHUT_RD);
        close(iSocket);
    }
}

int SocketSendData(int iSocket, const unsigned char * ucBuffer, int iLength)
{
    // sanity check
    if(iSocket < 0) {
        return iSocket;
    }

    int iSentBytes = 0;
    while(iLength)
    {
        int result = send(iSocket, ucBuffer, iLength, 0);
        if(result <= 0) {
            return -1;
        }
        iSentBytes += result;
        ucBuffer += result;
        iLength -= result;
    }
    return iSentBytes;
}

int SocketRecvData(int iSocket, unsigned char * ucBuffer, int iLength)
{
    // sanity check
    if(iSocket < 0) {
        return iSocket;
    }

    return recv(iSocket, ucBuffer, iLength, 0);
}
