#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/time.h>

void *threadrecv(void *vargp);
void *threadCalc(void *vargp);

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("usage: ./client ip port\n");
        return 1;
    }
    // unsigned short port = 8010; // 服务器的端口号
    // char *server_ip = "133.224.32.6";
    // char *server_ip = "127.0.0.1";
    int sockfd = 0;
    int err_log = 0;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

    // addr.sin_port = htons(atoi(argv[2]));

    // server_addr.sin_port = htons(port);
    inet_aton(argv[1], &server_addr.sin_addr);
    server_addr.sin_port = htons(atoi(argv[2]));

    // inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // 创建通信端点：套接字
    if (sockfd < 0)
    {
        perror("socket");
        exit(-1);
    }

    err_log = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)); // 主动连接服务器
    if (err_log != 0)
    {
        perror("connect");
        close(sockfd);
        exit(-1);
    }

    pthread_t tid2;
    pthread_create(&tid2, NULL, threadCalc, &sockfd);
    pthread_join(tid2, NULL);

    close(sockfd);
    return 0;
}

void *threadCalc(void *vargp)
{
    int sockfd = *((int *)vargp);
    char recv_buf[128];
    char send_buf[64];
    struct timeval newtime;
    while (1)
    {
        // printf("开始循环:\n");
        // 第一步 发送同步请求
        memset(send_buf, 0, sizeof(send_buf));
        strcpy(send_buf, "SYNC");
        send(sockfd, send_buf, 4, 0);
        gettimeofday(&newtime, NULL);

        // 第二步 发送第一步请求时间
        memset(send_buf, 0, sizeof(send_buf));
        // printf("%ld,%ld\n", newtime.tv_sec, newtime.tv_usec);
        snprintf(send_buf, 32, "FOLL%ld,%ld", newtime.tv_sec, newtime.tv_usec);
        send(sockfd, send_buf, strlen(send_buf), 0);
        // printf("发送了:%s\n", send_buf);

        // 第三步 等待服务端请求
        memset(recv_buf, 0, sizeof(recv_buf));
        recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        gettimeofday(&newtime, NULL);
        // printf("服务端:%s\n", recv_buf);

        // 第四步 发送收到请求的时间
        snprintf(send_buf, 32, "DERP%ld,%ld", newtime.tv_sec, newtime.tv_usec);
        send(sockfd, send_buf, strlen(send_buf), 0);
        // printf("发送了:%s\n", send_buf);

        // chanhu!@#123

        // 第五步, 接收结果
        memset(recv_buf, 0, sizeof(recv_buf));
        recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        // gettimeofday(&newtime, NULL);
        printf("%s", recv_buf);

        sleep(1);
    }
}

void *threadrecv(void *vargp)
{
    int sockfd = *((int *)vargp);
    char recv_buf[512];
    char send_buf[4] = "___";
    printf("成功连接到服务器.\n");
    while (1)
    {
        recv(sockfd, recv_buf, sizeof(recv_buf), 0); // 接收服务器发回的信息
        // printf("服务端：%s\n", recv_buf);
        if (0 == strcmp(recv_buf, "..."))
        {
            send(sockfd, send_buf, 3, 0);
        }
        memset(recv_buf, 0, sizeof(recv_buf));
    }

    return NULL;
}