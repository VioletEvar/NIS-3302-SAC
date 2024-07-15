#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUF_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char send_buffer[MAX_BUF_SIZE] = "Hello, Server!";
    char recv_buffer[MAX_BUF_SIZE];

    // 创建套接字
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址信息
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // 连接到服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // 发送数据到服务器
    sendto(sockfd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    printf("Message sent to server: %s\n", send_buffer);

    // 接收服务器的响应
    recvfrom(sockfd, recv_buffer, MAX_BUF_SIZE, 0, NULL, NULL);
    printf("Message received from server: %s\n", recv_buffer);

    // 关闭套接字
    close(sockfd);

    return 0;
}
