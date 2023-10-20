/*
 * @Description: lradc app
 * @Author: TOTHTOT
 * @Date: 2023-10-04 19:32:59
 * @LastEditTime: 2023-10-16 17:25:32
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/05_lradc/lradc_app.c
 */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/input.h>

#define DEV_PATH "/dev/input/event0" // device path

/* 类型定义 */

// app 结构体
typedef struct dht11_app
{
    int fd;                  // 设备描述符
    uint8_t exit_while_flag; // == 1退出主循环
    struct input_event ipt_ent_st;
} lradc_key_app_t;

lradc_key_app_t g_lradc_kye_app_st = {0};

int main(int argc, char *argv[])
{
    int32_t ret = 0;

    g_lradc_kye_app_st.fd = open(DEV_PATH, O_RDWR);
    if (g_lradc_kye_app_st.fd < 0)
    {
        printf("open() fail\n");
        return 1;
    }
    while (g_lradc_kye_app_st.exit_while_flag == 0)
    {
        ret = read(g_lradc_kye_app_st.fd, &g_lradc_kye_app_st.ipt_ent_st, sizeof(struct input_event));
        if (ret < 0)
        {
            printf("read() fail[%d]\n", ret);
        }
        else
        {
            if (g_lradc_kye_app_st.ipt_ent_st.type == EV_KEY)
            {
                printf("Event type: %d, code: %d, value: %d\n", g_lradc_kye_app_st.ipt_ent_st.type, g_lradc_kye_app_st.ipt_ent_st.code, g_lradc_kye_app_st.ipt_ent_st.value);
            }
        }

        usleep(500000);
    }

    return 0;
}