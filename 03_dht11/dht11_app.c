/*
 * @Description: dht11 app
 * @Author: TOTHTOT
 * @Date: 2023-10-04 19:32:59
 * @LastEditTime: 2023-10-20 10:44:57
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/03_dht11/dht11_app.c
 */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/epoll.h>

#define DHT11_DEV_PATH "/dev/dht11" // rgbled device path

/* 类型定义 */
// dht11 传感器数据类型
typedef struct dht11_data
{
    uint16_t temp;
    uint16_t humi;
} dht11_data_t;
// app 结构体
typedef struct dht11_app
{
    int dht11_fd;       // 设备描述符
    int dht11_epoll_fd; // epoll 文件描述符
#define EPOLL_MAX_NUM 2
    struct epoll_event ep_event[EPOLL_MAX_NUM];
    struct epoll_event dht11_ep_event;

    uint8_t exit_while_flag;           // == 1退出主循环
    dht11_data_t dht11_sensor_data_st; // dht11 数据
} dht11_app_t;

dht11_app_t g_dht11_app_st = {0};

int main(int argc, char *argv[])
{
    int32_t ret = 0;

    g_dht11_app_st.dht11_fd = open(DHT11_DEV_PATH, O_RDWR);
    if (g_dht11_app_st.dht11_fd < 0)
    {
        printf("dht11 open() fail\n");
        return 1;
    }
    g_dht11_app_st.dht11_epoll_fd = epoll_create(1);
    if (g_dht11_app_st.dht11_epoll_fd < 0)
    {
        printf("epoll_create() fail\n");
        return 2;
    }
    g_dht11_app_st.dht11_ep_event.events = EPOLLIN; // 监视可读事件
    g_dht11_app_st.dht11_ep_event.data.fd = g_dht11_app_st.dht11_fd;
    if (epoll_ctl(g_dht11_app_st.dht11_epoll_fd, EPOLL_CTL_ADD, g_dht11_app_st.dht11_fd, &g_dht11_app_st.dht11_ep_event) == -1)
    {
        perror("epoll_ctl()");
        return 1;
    }
    usleep(10000);
    while (g_dht11_app_st.exit_while_flag == 0)
    {
        int ep_ret = 0;
        ep_ret = epoll_wait(g_dht11_app_st.dht11_epoll_fd, g_dht11_app_st.ep_event, EPOLL_MAX_NUM, 0); // 等待监听的事件发生
        if (ep_ret > 0)
        {
            for (int32_t i = 0; i < ep_ret; i++) // 遍历事件, 如果发送的事件属于需要的就对相应的fd进行处理
            {
                if (g_dht11_app_st.ep_event[i].events & EPOLLIN) // 事件满足
                {
                    // fd 匹配成功
                    if (g_dht11_app_st.dht11_fd == g_dht11_app_st.ep_event[i].data.fd)
                    {
                        ret = read(g_dht11_app_st.dht11_fd, &g_dht11_app_st.dht11_sensor_data_st, sizeof(dht11_data_t));
                        if (ret < 0)
                        {
                            printf("read() fail[%d]\n", ret);
                        }
                        else
                        {
                            printf("dht11 data{temp = %d, humi = %d}\n", g_dht11_app_st.dht11_sensor_data_st.temp / 10, g_dht11_app_st.dht11_sensor_data_st.humi / 10);
                        }
                    }
                }
            }
        }
        else if(ep_ret < 0)
            printf("epoll_wait() error[%d]\n", ep_ret);
        usleep(100000); // 100ms
    }

    return 0;
}