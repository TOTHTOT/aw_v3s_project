/*
 * @Description: dht11 app
 * @Author: TOTHTOT
 * @Date: 2023-10-04 19:32:59
 * @LastEditTime: 2023-10-10 10:17:20
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
    int dht11_fd;                      // 设备描述符
    uint8_t exit_while_flag;           // == 1退出主循环
    dht11_data_t dht11_sensor_data_st; // dht11 数据
} dht11_app_t;

dht11_app_t g_dht11_app_st = {0};

int main(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t delay_time = atoi(argv[1]);

    g_dht11_app_st.dht11_fd = open(DHT11_DEV_PATH, O_RDWR);
    if (g_dht11_app_st.dht11_fd < 0)
    {
        printf("dht11 open() fail\n");
        return 1;
    }

    usleep(10000);
    while (g_dht11_app_st.exit_while_flag == 0)
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

        usleep(delay_time);
    }

    return 0;
}