/*
 * @Description: 
 * @Author: TOTHTOT
 * @Date: 2023-10-03 16:37:58
 * @LastEditTime: 2023-10-03 18:24:52
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/02_oled_0.96/oled_096_tty_app.c
 */
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#define DEV_PATH "/dev/tty1"
// 传入这个参数实现清空tty 关闭光标
// ./oled_096_tty_app $(clear) "$(echo -e "\033[?25l")" 22

typedef struct oled_096_tty_app
{
    int fd;
}oled_096_tty_app_t;

oled_096_tty_app_t g_oled_app_st = {0};

int main(int argc, char *argv[])
{
    if(argc < 4)
    {
        printf("argc error\n");
        return 2;
    }

    g_oled_app_st.fd = open(DEV_PATH, O_RDWR);
    if(g_oled_app_st.fd < 0)
    {
        printf("open() error\n");
        return 1;
    }
    write(g_oled_app_st.fd, argv[1], strlen(argv[1]));
    write(g_oled_app_st.fd, argv[2], strlen(argv[2]));
    write(g_oled_app_st.fd, argv[3], strlen(argv[3]));

    return 0;
}