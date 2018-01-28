#include <httpget.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int http_get(const char * host, const char * request, http_get_cb_t handle_data, void * callback_data)
{
    char buf[256];
    int send_size = snprintf(buf, sizeof(buf), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", request, host);
    if (send_size < 0 || sizeof(buf) <= send_size) {
        return 1;
    }

    struct hostent * hostinfo = gethostbyname(host);
    if (hostinfo == NULL || hostinfo->h_addrtype != AF_INET) {
        return 3;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(80);
    dest_addr.sin_addr = *(struct in_addr *) hostinfo->h_addr;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 4;
    }

    int rc = 0;

    if (connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        rc = 5;
        goto close_socket_and_leave;
    }

    int sent = 0;
    while (sent < send_size) {
        int sent_now = write(sock, buf + sent, send_size - sent);
        if (sent_now == 0)
            break;
        if (sent_now < 0) {
            rc = 6;
            goto close_socket_and_leave;
        }
        sent += sent_now;
    }

    int recv_size;
    while ((recv_size = read(sock, buf, sizeof(buf))) > 0) {
        handle_data(buf, recv_size, callback_data);
    }
    if (recv_size < 0) {
        rc = 7;
    }

close_socket_and_leave:
    close(sock);
    return rc;
}
