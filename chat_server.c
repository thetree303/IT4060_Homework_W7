#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_CLIENT 10
#define SERVER_PORT 8000

typedef struct {
    int fd;
    char name[64];
    int authenticated;
} Client;

void get_timestamp(char *buffer) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 25, "%Y/%m/%d %H:%M:%S", timeinfo);
}

int main() {
    // Khoi tao danh sach client
    Client clients[MAX_CLIENT];
    for (int i = 0; i < MAX_CLIENT; i++) {
        clients[i].fd = -1;
        clients[i].authenticated = 0;
    }

    // Tao socket
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed"); 
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port %d\n", SERVER_PORT);

    fd_set fdread;
    int max_fd;
    char buf[256];

    while (1) {
        FD_ZERO(&fdread);
        FD_SET(listener, &fdread);
        max_fd = listener;

        for (int i = 0; i < MAX_CLIENT; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &fdread);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        if (select(max_fd + 1, &fdread, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            continue;
        }

        if (FD_ISSET(listener, &fdread)) {
            int new_socket = accept(listener, NULL, NULL);
            if (new_socket < 0) {
                perror("accept() error");
            } else {
                for (int i = 0; i < MAX_CLIENT; i++) {
                    if (clients[i].fd == -1) {
                        clients[i].fd = new_socket;
                        clients[i].authenticated = 0;
                        send(new_socket, "Enter name (client_id: name): ", 30, 0);
                        break;
                    }
                }
            }
        }

        // Xu ly du lieu tu client
        for (int i = 0; i < MAX_CLIENT; i++) {
            int sd = clients[i].fd;
            if (sd > 0 && FD_ISSET(sd, &fdread)) {
                int valread = read(sd, buf, 256);
                if (valread <= 0) {
                    close(sd);
                    clients[i].fd = -1;
                    clients[i].authenticated = 0;
                } else {
                    buf[valread] = '\0';
                    buf[strcspn(buf, "\r\n")] = 0;

                    if (!clients[i].authenticated) {
                        // Kiem tra cu phap
                        char prefix[20], name[64];
                        if (sscanf(buf, "client_id: %s", name) == 1) {
                            strcpy(clients[i].name, name);
                            clients[i].authenticated = 1;
                            send(sd, "Success!\n", 9, 0);
                            printf("New client connected: %d\n", clients[i].fd);
                        } else {
                            send(sd, "Invalid format!\n", 16, 0);
                        }
                    } else {
                        // Gui tin nhan den cac client
                        char time_str[25];
                        char out_msg[512];
                        get_timestamp(time_str);
                        
                        snprintf(out_msg, sizeof(out_msg), "%s %s: %s\n", time_str, clients[i].name, buf);
                        
                        for (int j = 0; j < MAX_CLIENT; j++) {
                            if (clients[j].fd > 0 && clients[j].authenticated && i != j) {
                                send(clients[j].fd, out_msg, strlen(out_msg), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}