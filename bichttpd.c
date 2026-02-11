#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>

int debug_mode = 0;

int est_entier(const char *s) {
    for (int i = 0; s[i]; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

void worker(int client_sock, int debug_mode) {
    char *buffer = malloc(4096);
    if (!buffer) {
        perror("malloc");
        close(client_sock);
        return;
    }

    ssize_t received = recv(client_sock, buffer, 4095, 0);
    if (received < 0) {
        perror("recv");
        free(buffer);
        close(client_sock);
        return;
    }

    buffer[received] = '\0';

    if (debug_mode) {
        fprintf(stderr, "[DEBUG] Requête brute reçue :\n%s\n", buffer);
    }

    char method[8], path[256], version[16];
    sscanf(buffer, "%7s %255s %15s", method, path, version);

    time_t now = time(NULL);
    struct tm *local_tm = localtime(&now);
    char date_header[128];
    strftime(date_header, sizeof(date_header), "Date: %a, %d %b %Y %H:%M:%S %Z\r\n", local_tm);

    if ((strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0 || strcmp(method, "POST") == 0) &&
        strcmp(version, "HTTP/1.0") == 0) {

        if (strstr(path, "..")) {
            char response[512];
            snprintf(response, sizeof(response),
                "HTTP/1.1 403 Forbidden\r\n"
                "%s"
                "Content-Type: text/plain\r\n"
                "Content-Length: 24\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Accès interdit au chemin!\n", date_header);
            send(client_sock, response, strlen(response), 0);
        } else {
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "./www%s", path);

            FILE *f = fopen(file_path, "r");
            if (!f) {
                char response[512];
                snprintf(response, sizeof(response),
                    "HTTP/1.1 404 Not Found\r\n"
                    "%s"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 22\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "Fichier non trouvé!\n", date_header);
                send(client_sock, response, strlen(response), 0);
            } else {
                fseek(f, 0, SEEK_END);
                long filesize = ftell(f);
                rewind(f);

                char *file_content = malloc(filesize + 1);
                if (!file_content) {
                    perror("malloc");
                    fclose(f);
                    free(buffer);
                    close(client_sock);
                    return;
                }

                fread(file_content, 1, filesize, f);
                file_content[filesize] = '\0';
                fclose(f);

                char header[512];
                snprintf(header, sizeof(header),
                    "HTTP/1.1 200 OK\r\n"
                    "%s"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: %ld\r\n"
                    "Connection: close\r\n"
                    "\r\n", date_header, filesize);

                send(client_sock, header, strlen(header), 0);
                if (strcmp(method, "HEAD") != 0) {
                    send(client_sock, file_content, filesize, 0);
                }
                free(file_content);
            }
        }

    } else {
        char response[512];
        snprintf(response, sizeof(response),
            "HTTP/1.1 400 Bad Request\r\n"
            "%s"
            "Content-Type: text/plain\r\n"
            "Content-Length: 23\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Requête invalide!\n", date_header);
        send(client_sock, response, strlen(response), 0);
    }

    free(buffer);
    close(client_sock);
}

int main(int argc, char *argv[]) {
    int port = 42110;
    int opt;
    char config_path[256] = "";

    while ((opt = getopt(argc, argv, "p:d::c:")) != -1) {
        switch (opt) {
            case 'p':
                if (!est_entier(optarg)) {
                    fprintf(stderr, "Invalid argument: port must be a number\n");
                    exit(1);
                }
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid argument: port out of range\n");
                    exit(1);
                }
                break;
            case 'd':
                if (optarg == NULL || strcmp(optarg, "on") == 0) {
                    debug_mode = 1;
                } else if (strcmp(optarg, "off") == 0) {
                    debug_mode = 0;
                } else {
                    fprintf(stderr, "Invalid argument for -d: use 'on' or 'off'\n");
                    exit(1);
                }
                break;
            case 'c':
                strncpy(config_path, optarg, sizeof(config_path) - 1);
                config_path[sizeof(config_path) - 1] = '\0';
                break;
            default:
                fprintf(stderr, "Usage: %s -p <port> [-d on|off] [-c <config_file>]\n", argv[0]);
                exit(1);
        }
    }

    if (strlen(config_path) > 0) {
        FILE *config = fopen(config_path, "r");
        if (!config) {
            fprintf(stderr, "Erreur ouverture fichier de configuration : %s\n", config_path);
            exit(1);
        }

        char line[256];
        while (fgets(line, sizeof(line), config)) {
            if (strncmp(line, "port=", 5) == 0) {
                port = atoi(line + 5);
            } else if (strncmp(line, "debug=", 6) == 0) {
                if (strncmp(line + 6, "on", 2) == 0) debug_mode = 1;
                else if (strncmp(line + 6, "off", 3) == 0) debug_mode = 0;
            }
        }

        fclose(config);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(port);

    if (debug_mode) {
        fprintf(stderr, "[bichttpd] Trying to bind to 127.0.0.1:%d\n", port);
    }

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (debug_mode) {
        fprintf(stderr, "[bichttpd] Socket successfully bound to 127.0.0.1:%d\n", port);
    }

    listen(sockfd, 10);

    if (debug_mode) {
        fprintf(stderr, "[bichttpd] Listening on socket %d\n", sockfd);
    }

    printf("Serveur en écoute sur 127.0.0.1:%d\n", port);
    if (debug_mode) {
        fprintf(stderr, "[DEBUG] Mode débogage activé\n");
    }

    while (1) {
        while (waitpid(-1, NULL, WNOHANG) > 0);

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        if (debug_mode) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            fprintf(stderr, "[bichttpd] Connection accepted from %s:%d\n", client_ip, client_port);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_sock);
            continue;
        }

                if (pid == 0) {
            // Processus enfant
            close(sockfd);
            if (debug_mode) {
                fprintf(stderr, "[bichttpd] Subprocess %d handling request\n", getpid());
            }
            worker(client_sock, debug_mode);
            exit(0);
        } else {
            // Processus parent
            close(client_sock);
        }
    }

    close(sockfd);
    return 0;
}
