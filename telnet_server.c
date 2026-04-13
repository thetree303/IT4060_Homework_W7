#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

#define MAX_CLIENT 10
#define SERVER_PORT 8000
#define BUFFER_SIZE 1024
#define DB_FILE "db.txt"

typedef struct {
    int fd;
    int authenticated;
} Client;

int check_login(char* user, char* pass) {
    FILE *f = fopen(DB_FILE, "r");
    if (f == NULL) {
        perror("Could not open database file");
        return 0;
    }
    char db_user[64], db_pass[64];
    while (fscanf(f, "%s %s", db_user, db_pass) != EOF) {
        if (strcmp(user, db_user) == 0 && strcmp(pass, db_pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void send_file_content(int client_fd, char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        send(client_fd, "ERROR: Could not read command output.\n", 38, 0);
        return;
    }
    char file_buf[BUFFER_SIZE];
    while (fgets(file_buf, sizeof(file_buf), f) != NULL) {
        send(client_fd, file_buf, strlen(file_buf), 0);
    }
    fclose(f);
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

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Telnet Server listening on port %d\n", SERVER_PORT);

    fd_set fdread;
    int max_fd;
    char buf[BUFFER_SIZE];

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
                        send(new_socket, "Login (user pass): ", 20, 0);
                        break;
                    }
                }
            }
        }

        // Xu ly du lieu tu client
        for (int i = 0; i < MAX_CLIENT; i++) {
            int sd = clients[i].fd;
            if (sd > 0 && FD_ISSET(sd, &fdread)) {
                int n = read(sd, buf, sizeof(buf) - 1);
                if (n <= 0) {
                    close(sd);
                    clients[i].fd = -1;
                } else {
                    buf[n] = '\0';
                    buf[strcspn(buf, "\r\n")] = 0;

                    if (!clients[i].authenticated) {
                        // Kiem tra dang nhap
                        char user[64], pass[64];
                        if (sscanf(buf, "%s %s", user, pass) == 2) {
                            if (check_login(user, pass)) {
                                clients[i].authenticated = 1;
                                send(sd, "Login Success! Enter command: ", 31, 0);
                                printf("New client connected: %d\n", clients[i].fd);
                            } else {
                                send(sd, "Login Failed! Try again: ", 25, 0);
                            }
                        } else {
                            send(sd, "Invalid format! Try again: ", 27, 0);
                        }
                    } else {
                        // Thuc thi lenh, gui ket qua
                        char cmd[BUFFER_SIZE + 20];
                        sprintf(cmd, "%s > out.txt", buf);
                        system(cmd);
                        send_file_content(sd, "out.txt");
  
                        // Dong ket noi
                        close(sd);
                        clients[i].fd = -1;
                        clients[i].authenticated = 0;
                        printf("Client disconnected: %d\n", sd);
                    }
                }
            }
        }
    }
    return 0;
}