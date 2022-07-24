#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_USER_COUNT 128
#define SYNC 1
#define FOLL 2
#define DERE 3
#define DERP 4

static void listener_cb(struct evconnlistener *, evutil_socket_t, struct sockaddr *, int, void *);
static void conn_readcb(struct bufferevent *bev, void *arg);
static void signal_cb(evutil_socket_t, short, void *);
static void conn_eventcb(struct bufferevent *bev, short events, void *user_data);

typedef struct _addr_t
{
    char ip[16];
    int port;
    int index;
    struct timeval tv[4];
    long offset[10];
    long delay[10];
} addr_t;

int main(int argc, char const *argv[])
{
// 设置守护进程模式
#ifdef DAEMON
    daemon(1, 0);
#endif

    int serverPort = 8010;
    struct event_base *base;
    struct evconnlistener *listener;
    struct event *signal_event;

    // 创建根节点
    base = event_base_new();
    if (!base)
    {
        fprintf(stderr, "Could not initialize libevent!\n");
        return 1;
    }

    // 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverPort);
    addr.sin_addr.s_addr = htonl(0);
    socklen_t len = sizeof(addr);
    listener = evconnlistener_new_bind(base, listener_cb, (void *)base, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE_PORT, -1, (struct sockaddr *)&addr, len);
    if (!listener)
    {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }
    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

    if (!signal_event || event_add(signal_event, NULL) < 0)
    {
        fprintf(stderr, "Could not create/add a signal event!\n");
        return 1;
    }
    // 循环监听
    event_base_dispatch(base);
    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
}

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen, void *arg)
{
    struct event_base *base = (struct event_base *)arg;
    struct bufferevent *bev = NULL;
    struct sockaddr_in *clientaddr = (struct sockaddr_in *)addr;
    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev)
    {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }
    addr_t *caddr = (addr_t *)malloc(sizeof(addr_t));
    caddr->port = ntohs(clientaddr->sin_port);
    inet_ntop(AF_INET, &clientaddr->sin_addr, caddr->ip, sizeof(caddr->ip));
    caddr->index = 0;
    printf("New Connect: %s:%d\n", caddr->ip, caddr->port);
    bufferevent_setcb(bev, conn_readcb, NULL, conn_eventcb, caddr);
    bufferevent_enable(bev, EV_READ);
}

// 获得客户端请求类型
int getReqType(const char *sreq)
{
    if (0 == strcmp(sreq, "SYNC"))
    {
        // 客户端发送同步请求
        // gettimeofday(&start, NULL);
        return 1;
    }
    else if (0 == strcmp(sreq, "FOLL"))
    {
        // 客户端发送 同步请求的时间T1
        return 2;
    }
    else if (0 == strcmp(sreq, "DERE"))
    {
        // 服务算发送延迟请求 永远不会走到这里
        return 3;
    }
    else if (0 == strcmp(sreq, "DERP"))
    {
        // 客户端相应请求
        return 4;
    }
    else
    {
        // 未知请求
        return -1;
    }
}

static void conn_readcb(struct bufferevent *bev, void *arg)
{
    addr_t *addr = (addr_t *)arg;
    int n;

    char buffer[1024];
    // printf("转发:");
    while ((n = bufferevent_read(bev, buffer, sizeof(buffer))) > 0)
    {
        char req[5];
        strncpy(req, buffer, 4);
        req[4] = '\0';
        int type = getReqType(req);
        // printf("%d:%s\n", (int)strlen(buffer), buffer);
        // printf("type:%d\n", type);
        // printf("%s", buffer);
        // printf("Msg: %s:%d: %s", addr->ip, addr->port, buffer);

        switch (type)
        {
        case SYNC:
            // printf("同步请求!\n");
            gettimeofday(&addr->tv[1], NULL);
            // printf("%ld,%ld\n", addr->tv[1].tv_sec, addr->tv[1].tv_usec);
            char str_time[32];
            struct tm *p;
            p = localtime(&addr->tv[1].tv_sec);
            snprintf(str_time, 32, "%02d-%02d-%02d %02d:%02d:%02d.%06lu", p->tm_year + 1900,
                     p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, addr->tv[1].tv_usec);
            // printf("%s\n", str_time);
            break;
        case FOLL:
            // printf("返回同步时间!\n");
            sscanf(buffer, "FOLL%ld,%ld", &addr->tv[0].tv_sec, &addr->tv[0].tv_usec);
            char str_time1[32];
            struct tm *p1;
            p1 = localtime(&addr->tv[0].tv_sec);
            snprintf(str_time1, 32, "%02d-%02d-%02d %02d:%02d:%02d.%06lu", p1->tm_year + 1900,
                     p1->tm_mon + 1, p1->tm_mday, p1->tm_hour, p1->tm_min, p1->tm_sec, addr->tv[0].tv_usec);
            // printf("%s\n", str_time1);

            // 发送delay_Req请求
            bufferevent_write(bev, "DERE", 4);
            gettimeofday(&addr->tv[2], NULL);
            break;
        case DERE:
            break;
        case DERP:
            // printf("返回响应服务端时间!\n");
            // gettimeofday(&addr->tv[3], NULL);

            sscanf(buffer, "DERP%ld,%ld", &addr->tv[3].tv_sec, &addr->tv[3].tv_usec);

            int s = addr->tv[1].tv_sec + addr->tv[2].tv_sec - addr->tv[0].tv_sec - addr->tv[3].tv_sec;
            long us = addr->tv[1].tv_usec + addr->tv[2].tv_usec - addr->tv[0].tv_usec - addr->tv[3].tv_usec + s * 1000000;
            int offset = us / 2;

            int ds = addr->tv[1].tv_sec + addr->tv[3].tv_sec - addr->tv[0].tv_sec - addr->tv[2].tv_sec;
            long dus = addr->tv[1].tv_usec + addr->tv[3].tv_usec - addr->tv[0].tv_usec - addr->tv[2].tv_usec + ds * 1000000;
            int delay = dus / 2;

            addr->offset[addr->index] = offset;
            addr->delay[addr->index] = delay;
            char str_res[128];
            if (++addr->index == 10)
            {
                long of = 0, de = 0;
                for (int i = 0; i < 10; ++i)
                {
                    of += addr->offset[i];
                    de += addr->delay[i];
                }
                of /= 10;
                de /= 10;
                snprintf(str_res, 128, "%s:%d delay:%dus offset:%dus AVG: delay:%ldus offset:%ldus\n", addr->ip, addr->port, delay, offset, de, of);
                addr->index = 0;
            }
            else
            {
                snprintf(str_res, 128, "%s:%d delay:%dus offset:%dus\n", addr->ip, addr->port, delay, offset);
            }
            // INSERT INTO `offset` VALUES(NULL,"127.0.0.1:44630",now(),12345,12445);
            bufferevent_write(bev, str_res, strlen(str_res));
            printf("%s", str_res);
        default:
            break;
        }

        // bufferevent_write(bev, buffer, strlen(buffer));
        memset(buffer, 0, sizeof(buffer));
    }
}

static void conn_eventcb(struct bufferevent *bev, short events, void *arg)
{
    addr_t *addr = (addr_t *)arg;
    if (events & BEV_EVENT_EOF)
    {
        printf("Close: %s:%d\n", addr->ip, addr->port);
    }
    else if (events & BEV_EVENT_ERROR)
    {
        printf("Got an error on the connection: %s\n",
               strerror(errno));
    }
    /* None of the other events can happen here, since we haven't enabled
     * timeouts */
    free(arg);
    bufferevent_free(bev);
}

static void
signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    struct event_base *base = user_data;
    struct timeval delay = {0, 0};

    // printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

    event_base_loopexit(base, &delay);
}
/*
2022-07-13 08:58:01.557903
2022-07-13 08:58:01.557916
2022-07-13 08:58:01.558050
2022-07-13 08:58:01.558140
*/