/*
 * @Description: framebuffer 测试项目
 * @Author: TOTHTOT
 * @Date: 2024-02-18 21:02:12
 * @LastEditTime: 2024-02-18 22:48:13
 * @LastEditors: TOTHTOT
 * @FilePath: /project/06_framebuffer/framebuffer.c
 */
#include <stdio.h>
#include <stdint.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#define FB_DEV_PATH "/dev/fb0" // fb设备地址

typedef struct framebuffer_test
{
    int32_t fb_fd;                        // fb的文件描述符
    struct fb_fix_screeninfo fb_info_st;  // 设备固定参数
    struct fb_var_screeninfo fb_vinfo_st; // 获取虚拟参数
    uint32_t byte_per_pixel;              // 一个像素占用的字节
    uint32_t fb_w_byte, fb_h_byte;        // 用字节表示的宽度和高度
    uint64_t fb_buf_size;                 // framebuffer大小
    int8_t *fb_mmp_p;                     // 内存映射地址
    uint8_t is_run;
} framebuffer_test_t;

framebuffer_test_t fb_test_st = {0};

void fillRect(framebuffer_test_t *fb_info, int color, int x, int y, int w, int h)
{
    int i = 0, j = 0, offset = 0;
    if (fb_info->byte_per_pixel == 4)
    {
        for (i = 0; i < h; i++)
        {
            offset = x * fb_info->byte_per_pixel + (y + i) * fb_info->fb_w_byte;
            for (j = 0; j < w; j++)
            {
                int *tmp = (int *)&fb_info->fb_mmp_p[offset];
                *tmp = color;
                offset += fb_info->byte_per_pixel;
            }
        }
    }
    else
        printf("error bytesPerPixel=%d\n", fb_info->byte_per_pixel);
}

void sig_num(int32_t sig_num)
{
    switch (sig_num)
    {
    case SIGINT:
        fb_test_st.is_run = 0;
        break;
    
    default:
        break;
    }
}
int main(void)
{
    int32_t ret = 0;

    fb_test_st.is_run = 1;
    
    signal(SIGINT, sig_num);

    // 打开设备
    fb_test_st.fb_fd = open(FB_DEV_PATH, O_RDWR);
    if (fb_test_st.fb_fd < 0)
    {
        fprintf(stderr, "error: fb open() fail\n");
        return 1;
    }

    // 获取fb信息
    ret = ioctl(fb_test_st.fb_fd, FBIOGET_FSCREENINFO, &fb_test_st.fb_info_st);
    if (ret < 0)
    {
        fprintf(stderr, "get FBIOGET_FSCREENINFO ioctl() fail [%d]\n", ret);
        ret = 2;
        goto ERR_CLOSE;
    }
    ret = ioctl(fb_test_st.fb_fd, FBIOGET_VSCREENINFO, &fb_test_st.fb_vinfo_st);
    if (ret < 0)
    {
        fprintf(stderr, "get FBIOGET_VSCREENINFO ioctl() fail [%d]\n", ret);
        ret = 3;
        goto ERR_CLOSE;
    }

    fb_test_st.byte_per_pixel = fb_test_st.fb_vinfo_st.bits_per_pixel / 8;
    fb_test_st.fb_vinfo_st.xres = fb_test_st.fb_vinfo_st.xres_virtual = fb_test_st.fb_info_st.line_length / fb_test_st.byte_per_pixel;
    ret = ioctl(fb_test_st.fb_fd, FBIOPUT_VSCREENINFO, &fb_test_st.fb_vinfo_st);
    if (ret < 0)
    {
        ret = 4;
        fprintf(stderr, "put FBIOPUT_VSCREENINFO ioctl() fail [%d]\n", ret);
        goto ERR_CLOSE;
    }

    fb_test_st.fb_h_byte = fb_test_st.fb_vinfo_st.yres * fb_test_st.byte_per_pixel;
    fb_test_st.fb_w_byte = fb_test_st.fb_vinfo_st.xres * fb_test_st.byte_per_pixel;
    printf("[line:%d] line_length=%d res=[%dx%d], Bpp=%d ByteWH=[%d %d]\n", __LINE__, fb_test_st.fb_info_st.line_length, fb_test_st.fb_vinfo_st.xres,
           fb_test_st.fb_vinfo_st.yres, fb_test_st.byte_per_pixel, fb_test_st.fb_w_byte, fb_test_st.fb_h_byte);

    fb_test_st.fb_buf_size = fb_test_st.fb_vinfo_st.xres * fb_test_st.fb_vinfo_st.yres * fb_test_st.byte_per_pixel;
    fb_test_st.fb_mmp_p = mmap(0, fb_test_st.fb_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_test_st.fb_fd, 0);
    if (fb_test_st.fb_mmp_p == NULL)
    {
        fprintf(stderr, "mmap() fail\n");
        ret = 5;
        goto ERR_CLOSE;
    }

    while (fb_test_st.is_run)
    {
        fillRect(&fb_test_st, 0xff0000, 0, 0, 30, 30); // 红色
        sleep(1);
        fillRect(&fb_test_st, 0x00ff00, 0, 0, 30, 30); // 绿色
        sleep(1);
        fillRect(&fb_test_st, 0x0000ff, 0, 0, 30, 30); // 蓝色
        sleep(1);
    }

    close(fb_test_st.fb_fd);
    munmap(fb_test_st.fb_mmp_p, fb_test_st.fb_buf_size);
    printf("app exit success\n");
    return 0;

ERR_CLOSE:
    close(fb_test_st.fb_fd);
    return ret;
}