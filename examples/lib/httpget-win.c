#include <httpget.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

static WSADATA wsa;

int http_get(const char * host, const char * request, http_get_cb_t handle_data, void * callback_data)
{   
    char buf[256];
    int send_size = snprintf(buf, sizeof(buf), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", request, host);
    if (send_size < 0 || sizeof(buf) <= send_size) {
        return 1;
    }
    
    if (wsa.wVersion == 0 && WSAStartup(WINSOCK_VERSION, &wsa) != 0) {
        return 2;
    }

    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;
    struct addrinfo * dest;
    
    if (getaddrinfo(host, "80", &hint, &dest) != 0) {
        return 3;
    }

    int rc = 4;    

    SOCKET sock = socket(dest->ai_family, dest->ai_socktype, dest->ai_protocol);
    if (sock == INVALID_SOCKET) {
        goto free_addrinfo_and_leave;
    }

    if (connect(sock, dest->ai_addr, dest->ai_addrlen) == SOCKET_ERROR) {
        rc = 5;
        goto close_socket_and_leave;
    }

    if (send(sock, buf, send_size, 0) == SOCKET_ERROR) {
        rc = 6;
        goto close_socket_and_leave;
    }

    int recv_size;
    while ((recv_size = recv(sock, buf, sizeof(buf), 0)) > 0) {
        handle_data(buf, recv_size, callback_data);
    }
    if (recv_size == 0) {
        rc = 0;
    } else if (recv_size < 0) {
        rc = 7;
    }

close_socket_and_leave:
    closesocket(sock);
free_addrinfo_and_leave:
    freeaddrinfo(dest);
    return rc;
}
