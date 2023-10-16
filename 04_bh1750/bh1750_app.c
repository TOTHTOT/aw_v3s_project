/*
 * @Description: dht11 app
 * @Author: TOTHTOT
 * @Date: 2023-10-04 19:32:59
 * @LastEditTime: 2023-10-16 10:28:07
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/04_bh1750/bh1750_app.c
 */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DHT11_DEV_PATH "/dev/bh1750" // device path

/* 类型定义 */
// dht11 传感器数据类型
typedef struct bh1750_data
{
    // data
    uint16_t lux;
} bh1750_data_t;

// app 结构体
typedef struct dht11_app
{
    int fd;                       // 设备描述符
    uint8_t exit_while_flag;      // == 1退出主循环
    bh1750_data_t sensor_data_st; // 传感器数据
} bh1750_app_t;

bh1750_app_t g_bh1750_app_st = {0};

int main(int argc, char *argv[])
{
    int32_t ret = 0;

    g_bh1750_app_st.fd = open(DHT11_DEV_PATH, O_RDWR);
    if (g_bh1750_app_st.fd < 0)
    {
        printf("open() fail\n");
        return 1;
    }
    while (g_bh1750_app_st.exit_while_flag == 0)
    {
        ret = read(g_bh1750_app_st.fd, &g_bh1750_app_st.sensor_data_st, sizeof(bh1750_data_t));
        if (ret < 0)
        {
            printf("read() fail[%d]\n", ret);
        }
        else
        {
            printf("bh1750 data{temp = %f lux}\n", (float)(g_bh1750_app_st.sensor_data_st.lux / 1.2));
        }

        usleep(500000);
    }

    return 0;
}