/*
 * @Description: rgbleds app
 * @Author: TOTHTOT
 * @Date: 2023-09-27 16:53:39
 * @LastEditTime: 2023-09-29 10:18:11
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
#include <stdlib.h>

#define RGBLEDS_DEV_PATH "/dev/rgbled"  // rgbled device path
#define TEST_LED_COL 0  // open test led col

// rgbled状态, 必须保持xxx_on是奇数, xxx_off是偶数,且xxx_on在对应的xxx_off前面
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
 * @name: rgbled_toggle
 * @msg: toggle rgbled status
 * @param {rgbled_app_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-29 09:52:56
 */
uint8_t rgbled_toggle(rgbled_app_t *p_dev_st)
{
    if(p_dev_st->rgbled_status> RGBLED_STATUS_NONE&&p_dev_st->rgbled_status < RGBLED_MAX_STATUS)
    {
        // 如果是奇数表明是xxx_off的,减1状态推到xxx_on
        if(p_dev_st->rgbled_status % 2==0)
            p_dev_st->rgbled_status--;  // xxx_on
        else 
            p_dev_st->rgbled_status++;  //xxx_off

        write(p_dev_st->rgbleds_fd, &p_dev_st->rgbled_status, sizeof(rgbled_status_t));
        return 0;
    }
    else
        return 1;
}

/**
 * @name: rgbled_test_all_col
 * @msg: test rgb led light color
 * @param {rgbled_app_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-29 10:16:30
 */
void rgbled_test_all_col(rgbled_app_t *p_dev_st)
{
    rgbled_status_t led_status = RGBLED_STATUS_NONE;
    while (led_status!=RGBLED_MAX_STATUS)
    {
      write(p_dev_st->rgbleds_fd, &led_status, sizeof(rgbled_status_t));
      led_status++;
      usleep(300000);   // 300ms
    }
    
}

/**
 * @name: main
 * @msg: main process
 * @param {int} argc
 * @param {char} *argv
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-29 08:29:12
 */
int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("app arg number error!\n");
        return 1;
    }
    g_rgbleds_dev_t.rgbled_status = (rgbled_status_t)atoi(argv[1]);
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
        #if (TEST_LED_COL == 1)
        rgbled_test_all_col(&g_rgbleds_dev_t);
        #else
        if(rgbled_toggle(&g_rgbleds_dev_t) != 0)
        {
            printf("rgbled_toggle() fail!rgbleds_status = [%d]\n",g_rgbleds_dev_t.rgbled_status);
        }
        usleep(200000);  // sleep 200ms
        #endif // TEST_LED_COL
    }
    
    end_app(&g_rgbleds_dev_t);
    return 0;
}