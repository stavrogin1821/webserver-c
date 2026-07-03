/*
Зависимости: aiogram 3.x, aiohttp, BeautifulSoup4
*/







#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 4096

// Простейший парсер MIME-типа по расширению
const char *get_mime(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".html") == 0) return "text/html";
    if (strcasecmp(ext, ".css") == 0) return "text/css";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, ".png") == 0) return "image/png";
    return "text/plain";
}

// Отправить HTTP-ответ
void send_response(int client_fd, int status, const char *content_type, const char *body, int body_len) {
    char header[BUFFER_SIZE];
    const char *status_text = (status == 200) ? "OK" : "Not Found";
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n",
             status, status_text, content_type, body_len);
    write(client_fd, header, strlen(header));
    if (body && body_len > 0) {
        write(client_fd, body, body_len);
    }
}

// Обработать GET-запрос
void handle_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        close(client_fd);
        exit(0);
    }
    buffer[bytes] = '\0';

    // Разбираем первую строку
    char method[16], path[256];
    if (sscanf(buffer, "%15s %255s", method, path) != 2) {
        send_response(client_fd, 400, "text/plain", "Bad Request", 11);
        close(client_fd);
        exit(0);
    }

    if (strcasecmp(method, "GET") != 0) {
        send_response(client_fd, 405, "text/plain", "Method Not Allowed", 19);
        close(client_fd);
        exit(0);
    }

    // Убираем ведущий слеш, подставляем index.html если пусто
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // Безопасность: предотвращаем выход за пределы текущей директории
    if (strstr(path, "..") != NULL) {
        send_response(client_fd, 403, "text/plain", "Forbidden", 9);
        close(client_fd);
        exit(0);
    }

    // Открываем файл
    char filepath[512] = ".";
    strncat(filepath, path, sizeof(filepath) - 2);
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        const char *not_found = "<html><body><h1>404 Not Found</h1></body></html>";
        send_response(client_fd, 404, "text/html", not_found, strlen(not_found));
        close(client_fd);
        exit(0);
    }

    // Читаем и отправляем содержимое
    struct stat st;
    fstat(file_fd, &st);
    char *body = malloc(st.st_size);
    read(file_fd, body, st.st_size);
    close(file_fd);

    send_response(client_fd, 200, get_mime(filepath), body, st.st_size);
    free(body);
    close(client_fd);
    exit(0);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    // Игнорируем зомби-процессы
    signal(SIGCHLD, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Сервер слушает порт %d\n", PORT);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        pid_t pid = fork();
        if (pid == 0) {
            // Дочерний процесс обрабатывает запрос
            close(server_fd);
            handle_request(client_fd);
            exit(0);
        } else {
            close(client_fd);
        }
    }
    return 0;
}