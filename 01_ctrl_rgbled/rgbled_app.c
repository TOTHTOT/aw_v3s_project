/*
 * @Description: rgbleds app
 * @Author: TOTHTOT
 * @Date: 2023-09-27 16:53:39
 * @LastEditTime: 2023-09-28 17:30:33
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/01_ctrl_rgbled/rgbled_app.c
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define RGBLEDS_DEV_PATH "/dev/rgbled"  // rgbled device path


typedef enum
{
    RGBLED_STATUS_NONE,
    RGBLED_STATUS_BLUE_ON,
    RGBLED_STATUS_BLUE_OFF,
    RGBLED_STATUS_GREE_ON,
    RGBLED_STATUS_GREE_OFF,
    RGBLED_STATUS_RED_ON,
    RGBLED_STATUS_RED_OFF,
    RGBLED_STATUS_YELLOW_ON,
    RGBLED_STATUS_YELLOW_OFF,
    RGBLED_STATUS_CYAN_ON,
    RGBLED_STATUS_CYAN_OFF,
    RGBLED_STATUS_MAGENTA_ON,
    RGBLED_STATUS_MAGENTA_OFF,
    RGBLED_STATUS_ALL_ON,
    RGBLED_STATUS_ALL_OFF,
    RGBLED_MAX_STATUS
} rgbled_status_t;
typedef struct rgbled_app
{
    int rgbleds_fd;
    int32_t exit_flag;
    rgbled_status_t rgbled_status;
}rgbled_app_t;

rgbled_app_t g_rgbleds_dev_t = {0};
/**
 * @name: sig_handle
 * @msg: sig deal handle
 * @param {int} sig_num
 * @return {void}
 * @author: TOTHTOT
 * @Date: 2023-09-28 16:15:27
 */
void sig_handle(int sig_num)
{
    switch (sig_num)
    {
    case SIGINT:
        printf("recv SIGINT signal\n");
        g_rgbleds_dev_t.exit_flag = 1;
        break;
    
    default:
        break;
    }
}

/**
 * @name: end_app
 * @msg: deal exit app 
 * @param {rgbled_app_t} *p_rgbled_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-28 16:24:15
 */
int32_t end_app(rgbled_app_t *p_rgbled_st)
{
    close(p_rgbled_st->rgbleds_fd);
    printf("exit app\n");
    return 0;
}

/**
 * @name: main
 * @msg: 
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-28 16:25:35
 */
int main(void)
{
    signal(SIGINT, sig_handle);
    g_rgbleds_dev_t.rgbleds_fd = open(RGBLEDS_DEV_PATH, O_RDWR);
    if(g_rgbleds_dev_t.rgbleds_fd < 0)
    {
        printf("open() file fail\n");
        return -1;
    }

    printf("open rgbled device success!\n");
    while (g_rgbleds_dev_t.exit_flag == 0)
    {  
        g_rgbleds_dev_t.rgbled_status = RGBLED_STATUS_ALL_ON;
        write(g_rgbleds_dev_t.rgbleds_fd, &g_rgbleds_dev_t.rgbled_status, sizeof(rgbled_status_t));
        usleep(200000);  // sleep 200ms
        g_rgbleds_dev_t.rgbled_status = RGBLED_STATUS_ALL_OFF;
        write(g_rgbleds_dev_t.rgbleds_fd, &g_rgbleds_dev_t.rgbled_status, sizeof(rgbled_status_t));
        usleep(200000);  // sleep 200ms
    }
    
    end_app(&g_rgbleds_dev_t);
    return 0;
}