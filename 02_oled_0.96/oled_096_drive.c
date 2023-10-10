/*
 * @Description:
 * @Author: TOTHTOT
 * @Date: 2023-09-29 11:47:00
 * @LastEditTime: 2023-10-06 17:09:02
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/02_oled_0.96/oled_096_drive.c
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/fb.h>

#define DEVICE_DEV_NUM 1
#define DEVICE_NAME "oled_096"                // 显示到 /dev下的设备名字
#define DEVTREE_NODE_NAME "/rgb_leds"         // 设备树中节点名称
#define DEVTREE_NODE_IO_INFO "gpio_rgbleds"   // 设备树中节点io info
#define DEVTREE_NODE_IO_REQUST_NAME "OLED096" // 设备树中节点io requst nema

#define DEVTREE_NODE_STATUS "status"                 // 设备树中节点status
#define DEVTREE_NODE_STATUS_OK "okay"                // 设备树中节点status == okay
#define DEVTREE_NODE_COMPAT "compatible"             // 设备树中节点compatible
#define DEVTREE_NODE_COMPAT_INFO "tothtot,oled_0_96" // 设备树中节点compatible

// oled 屏幕参数
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_CMD_ADDR 0x80  // 发送指令的地址
#define OLED_DATA_ADDR 0x40 // 发送数据的地址
#define OLED_REFRESHRATE 1
#define OLED_MAX_CONTRAST 255   // 最大对比度

// oled 指令
#define OLED_CMD_DISPLAY_OFF 0xae
#define OLED_CMD_DISPLAY_ON 0xaf

/* 类型定义 */
typedef struct oled_0_96_drive
{
    int oled_fd;
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major; // dev num
    int minor; // dev num

    struct i2c_client *client;  // i2c 设备
    uint32_t contrast;  // 对比度
    // uint8_t *mmap_buffer;
    struct fb_info *info; // frame buffer
} oled_0_96_device_t;

// oled 发送数据内容
typedef struct oled_array
{
    uint8_t type;
    uint8_t data[0];
} oled_array_t;

/* 函数声明 */
static int oled_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int oled_remove(struct i2c_client *client);
static int oledfb_get_brightness(struct backlight_device *bdev);
static int oledfb_check_fb(struct backlight_device *bdev, struct fb_info *info);
static int oledfb_update_bl(struct backlight_device *bdev);

/* 变量声明 */
static struct file_operations g_oled_fops_st;
static struct fb_ops oled_fbops;
/* 变量定义 */
oled_0_96_device_t g_oled_0_96_dev_st = {0};
// oled 屏幕参数信息
static const struct fb_fix_screeninfo oled_fb_fix = {
    .id = "Solomon SSD1306",
    .type = FB_TYPE_PACKED_PIXELS,
    .visual = FB_VISUAL_MONO10,
    .xpanstep = 0,
    .ypanstep = 0,
    .ywrapstep = 0,
    .accel = FB_ACCEL_NONE,
};
static const struct fb_var_screeninfo oled_fb_var = {
    .xres = OLED_WIDTH,         // 屏幕宽
    .xres_virtual = OLED_WIDTH, // 虚拟屏幕宽、高
    .yres = OLED_HEIGHT,        // 高
    .yres_virtual = OLED_HEIGHT,
    .bits_per_pixel = 1, // 每像素点占用多少位
    .red.length = 1,
    .red.offset = 0,
    .green.length = 1,
    .green.offset = 0,
    .blue.length = 1,
    .blue.offset = 0,
};
static const struct backlight_ops oled_fb_bl_ops = {
	.options	= BL_CORE_SUSPENDRESUME,
	.update_status	= oledfb_update_bl,
	.get_brightness	= oledfb_get_brightness,
	.check_fb	= oledfb_check_fb,
};
static struct file_operations g_oled_fops_st = {
    .owner = THIS_MODULE,
    // .open = led_open,
    // .read = led_read,
    // .write = led_write,
    // .release = led_release,
};
/**
 * @name: oled_writebyte
 * @msg: i2c 发送数据给 oled
 * @param {i2c_client} *client
 * @param {uint8_t} addr
 * @param {uint8_t} data
 * @return {i2c_master_send()}
 * @author: TOTHTOT
 * @Date: 2023-10-01 11:43:44
 */
static int oled_writebyte(struct i2c_client *client, uint8_t addr, uint8_t data)
{
    uint8_t buf[] = {addr, data};
    return i2c_master_send(client, buf, 2);
};
/**
 * @name: oled_sendcmd
 * @msg: 发送命令给 oled
 * @param {i2c_client} *client
 * @param {uint8_t} data
 * @return {0} 成功
 * @return {-1} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-01 11:47:44
 */
static inline int oled_sendcmd(struct i2c_client *client, uint8_t cmd)
{
    int32_t ret = 0;
    ret = oled_writebyte(client, OLED_CMD_ADDR, cmd);
    if (ret < 0)
    {
        printk(KERN_ERR "oled_sendcmd() error[%d]\n", ret);
        return -1;
    }
    return 0;
}

/**
 * @name: oled_senddata
 * @msg: 发送数据给 oled
 * @param {i2c_client} *client
 * @param {uint8_t} data
 * @return {0} 成功
 * @return {-1} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-01 11:49:42
 */
static inline int oled_senddata(struct i2c_client *client, uint8_t data)
{
    int32_t ret = oled_writebyte(client, OLED_DATA_ADDR, data);
    if (ret < 0)
    {
        printk(KERN_ERR "oled_senddata() error[%d]\n", ret);
        return -1;
    }
    return 0;
}

/**
 * @name: oled_fb_alloc_array
 * @msg: 申请一块内存
 * @param {u32} len
 * @param {u8} type
 * @return {!= NULL} 成功
 * @author: TOTHTOT
 * @Date: 2023-10-02 17:58:17
 */
static oled_array_t *oled_fb_alloc_array(u32 len, u8 type)
{
    oled_array_t *array;

    array = kzalloc(sizeof(oled_array_t) + len, GFP_KERNEL);
    if (!array)
        return NULL;

    array->type = type;

    return array;
}
/**
 * @name: oled_send_datas
 * @msg: 发送一组数据给 oled
 * @param {i2c_client} *client
 * @param {oled_array_t} *arry
 * @param {u32} len
 * @return {0} 成功
 * @return {1} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-02 17:56:36
 */
static int32_t oled_send_datas(struct i2c_client *client, oled_array_t *arry, u32 len)
{
    int ret;

    len += sizeof(oled_array_t);

    ret = i2c_master_send(client, (uint8_t *)arry, len);
    if (ret != len)
    {
        printk(KERN_ERR "Couldn't send I2C command.[%d]\n", ret);
        return 1;
    }

    return 0;
}

/**
 * @name: oled_update_display
 * @msg:
 * @param {oled_0_96_device_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-03 09:27:51
 */
static void oled_update_display(oled_0_96_device_t *p_dev_st)
{
    oled_array_t *array;
    uint8_t *vmem = p_dev_st->info->screen_base;
    int i, j, k;

    array = oled_fb_alloc_array(OLED_WIDTH * OLED_HEIGHT / 8,
                                OLED_DATA_ADDR);
    if (!array)
        return;

    for (i = 0; i < (OLED_HEIGHT / 8); i++)
    {
        for (j = 0; j < OLED_WIDTH; j++)
        {
            u32 array_idx = i * OLED_WIDTH + j;
            array->data[array_idx] = 0;
            for (k = 0; k < 8; k++)
            {
                u32 page_length = OLED_WIDTH * i;
                u32 index = page_length + (OLED_WIDTH * k + j) / 8;
                u8 byte = *(vmem + index);
                u8 bit = byte & (1 << (j % 8));
                bit = bit >> (j % 8);
                array->data[array_idx] |= bit << k;
            }
        }
    }
    oled_send_datas(p_dev_st->client, array, OLED_WIDTH * OLED_HEIGHT / 8);
    kfree(array);
}

/**
 * @name: oled_arg_init
 * @msg: 初始化 oled 屏幕
 * @param {oled_0_96_device_t} *p_dev_st device
 * @return {0} success
 * @author: TOTHTOT
 * @Date: 2023-09-30 11:41:41
 */
static int32_t oled_arg_init(oled_0_96_device_t *p_dev_st)
{
    oled_sendcmd(p_dev_st->client, 0x81);
    oled_sendcmd(p_dev_st->client, 0x7f);

    oled_sendcmd(p_dev_st->client, 0xa1);
    oled_sendcmd(p_dev_st->client, 0xc8);

    oled_sendcmd(p_dev_st->client, 0xa8);
    oled_sendcmd(p_dev_st->client, OLED_HEIGHT -1);
    
    oled_sendcmd(p_dev_st->client, 0xd3);
    oled_sendcmd(p_dev_st->client, 0x00);

    oled_sendcmd(p_dev_st->client, 0xd5);
    oled_sendcmd(p_dev_st->client, 0x80);

    oled_sendcmd(p_dev_st->client, 0xd9);
    oled_sendcmd(p_dev_st->client, 0xf1);

    oled_sendcmd(p_dev_st->client, 0xda);
    oled_sendcmd(p_dev_st->client, 0x12);

    oled_sendcmd(p_dev_st->client, 0xdb);
    oled_sendcmd(p_dev_st->client, 0x40);

    oled_sendcmd(p_dev_st->client, 0x8d);
    oled_sendcmd(p_dev_st->client, 0x14);
    
    oled_sendcmd(p_dev_st->client, 0x20);
    oled_sendcmd(p_dev_st->client, 0x00); // 0x00

    oled_sendcmd(p_dev_st->client, 0x21);   // SSD1307FB_SET_COL_RANGE
    oled_sendcmd(p_dev_st->client, 0x00); 
    oled_sendcmd(p_dev_st->client, OLED_WIDTH -1); 

    oled_sendcmd(p_dev_st->client, 0x22);   // SSD1307FB_SET_PAGE_RANGE
    oled_sendcmd(p_dev_st->client, 0x00); // 0x00
    oled_sendcmd(p_dev_st->client, 7); 
    
    oled_sendcmd(p_dev_st->client, 0xaf);
    return 0;
}

/**
 * @name: oled_dev_init
 * @msg: 注册 oled 设备到内核
 * @param {oled_0_96_device_t} *p_dev_st device
 * @param {file_operations} *fops_p
 * @return {0} success
 * @author: TOTHTOT
 * @Date: 2023-09-30 11:41:39
 */
static int32_t oled_dev_init(oled_0_96_device_t *p_dev_st, struct file_operations *fops_p)
{
    int32_t ret = 0;

    /* 注册字符设备驱动 */
    ret = alloc_chrdev_region(&p_dev_st->devid, 0, DEVICE_DEV_NUM, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("%s get alloc_chrdev_region() error, ret = %d\n", DEVICE_NAME, ret);
        goto free_gpio;
    }
    // 设置设备号
    p_dev_st->major = MAJOR(p_dev_st->devid);
    p_dev_st->minor = MINOR(p_dev_st->devid);
    // 初始化 cdev
    p_dev_st->cdev.owner = THIS_MODULE;
    cdev_init(&p_dev_st->cdev, fops_p);
    if (cdev_add(&p_dev_st->cdev, p_dev_st->devid, DEVICE_DEV_NUM) < 0)
        goto del_unregister;

    p_dev_st->class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(p_dev_st->class))
        goto del_cdev;
    // 创建设备
    p_dev_st->device = device_create(p_dev_st->class, NULL, p_dev_st->devid, NULL, DEVICE_NAME);
    if (IS_ERR(p_dev_st->device))
        goto del_class;
    return 0;

del_class:
    class_destroy(p_dev_st->class);
del_cdev:
    cdev_del(&p_dev_st->cdev);
del_unregister:
    unregister_chrdev_region(p_dev_st->devid, DEVICE_DEV_NUM);
free_gpio:
    // gpio_free(p_dev_st->led_gpio_num[i]);

    return -EIO;
}

/**
 * @name:
 * @msg:
 * @param {fb_info} *info
 * @param {list_head} *pagelist
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-03 09:25:50
 */
static void ssd1307fb_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
    oled_update_display(info->par);
}

/**
 * @name:
 * @msg:
 * @param {i2c_client} *client
 * @param {i2c_device_id} *id
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-03 09:25:55
 */
static int32_t oled_fb_init(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct backlight_device *bl;
    struct device_node *node = client->dev.of_node;
    struct fb_info *info = NULL;
    struct fb_deferred_io *oled_fb_defio = NULL;
    oled_0_96_device_t *p_dev_st = NULL;
    int8_t bl_name[12];
    uint32_t vmem_size;
    uint8_t *vmem;
    int32_t ret;

    if (NULL == node)
    {
        printk(KERN_ERR "No device tree data found!\n");
        return -EINVAL;
    }

    info = framebuffer_alloc(sizeof(oled_0_96_device_t), &client->dev);
    if (!info)
    {
        printk(KERN_ERR "framebuffer_alloc() fail!\n");
        return -ENOMEM;
    }
    // 指针相互引用
    p_dev_st = info->par;
    p_dev_st->info = info;
    p_dev_st->client = client;

    // 显存分配内存
    vmem_size = OLED_WIDTH * OLED_HEIGHT / 8;
    vmem = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
                                    get_order(vmem_size));
    if (!vmem) // 错误处理
    {
        printk(KERN_ERR "Couldn't allocate graphical memory.\n");
        ret = -ENOMEM;
        goto fb_alloc_error;
    }

    oled_fb_defio = devm_kzalloc(&client->dev, sizeof(*oled_fb_defio), GFP_KERNEL);
    if (!oled_fb_defio)
    {
        printk(KERN_ERR "Couldn't allocate deferred io.\n");
        ret = -ENOMEM;
        goto fb_alloc_error;
    }

    oled_fb_defio->delay = HZ / OLED_REFRESHRATE;
    oled_fb_defio->deferred_io = ssd1307fb_deferred_io;

    // fb info 内容填充
    info->fbops = &oled_fbops;
    info->var = oled_fb_var;
    info->fix = oled_fb_fix;
    info->fix.line_length = OLED_WIDTH / 8;
    info->fbdefio = oled_fb_defio;
    info->screen_base = (uint8_t __force __iomem *)vmem; // 设置显存地址
    info->fix.smem_start = __pa(vmem);
    info->fix.smem_len = vmem_size;

    fb_deferred_io_init(info);
    i2c_set_clientdata(client, info);

    ret = register_framebuffer(info);
    if (ret)
    {
        printk(KERN_ERR "Couldn't register the framebuffer\n");
        goto fb_alloc_error;
    }
    snprintf(bl_name, sizeof(bl_name), "ssd1307fb%d", info->node);
    bl = backlight_device_register(bl_name, &client->dev, p_dev_st,
                                   &oled_fb_bl_ops, NULL);
    if (IS_ERR(bl))
    {
        ret = PTR_ERR(bl);
        dev_err(&client->dev, "unable to register backlight device: %d\n",
                ret);
        goto bl_init_error;
    }
    
    p_dev_st->contrast = 127;
    bl->props.brightness = p_dev_st->contrast;
    bl->props.max_brightness = OLED_MAX_CONTRAST;
    info->bl_dev = bl;

    dev_info(&client->dev, "fb%d: %s framebuffer device registered, using %d bytes of video memory\n", info->node, info->fix.id, vmem_size);

    return 0;
bl_init_error:
	unregister_framebuffer(info);
fb_alloc_error:
    framebuffer_release(info);
    return ret;
}
/**
 * @name: oled_probe
 * @msg: probe func
 * @param {i2c_client} *client
 * @param {i2c_device_id} *id
 * @return {0} success
 * @author: TOTHTOT
 * @Date: 2023-09-30 11:59:04
 */
static int oled_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int32_t ret = 0;

    // printk(KERN_INFO "oled probe success\n");

    g_oled_0_96_dev_st.client = client; // 赋值获取到的 设备信息从设备树中

    ret = oled_dev_init(&g_oled_0_96_dev_st, &g_oled_fops_st); // 注册设备到内核
    if (ret != 0)
        return 1;
    ret = oled_fb_init(client, id); // oled 的 framebuffer 初始化
    if (ret != 0)
        return 2;
    ret = oled_arg_init(&g_oled_0_96_dev_st); // 初始化 oled 屏幕参数
    if (ret != 0)
    {
    	fb_deferred_io_cleanup(g_oled_0_96_dev_st.info);
        framebuffer_release(g_oled_0_96_dev_st.info);
        return 3;
    }

    // printk(KERN_INFO "dev addr = %x\n", g_oled_0_96_dev_st.client->addr);
    return 0; // 返回 0 表示设备探测成功
}

/**
 * @name:oled_remove
 * @msg:
 * @param {i2c_client} *client
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-03 09:55:23
 */
static int oled_remove(struct i2c_client *client)
{
    struct fb_info *info = i2c_get_clientdata(client);
    // printk(KERN_INFO "oled remove start\n");

    oled_sendcmd(client, 0xAE);
    framebuffer_release(info);

    backlight_device_unregister(info->bl_dev);

    unregister_framebuffer(info);

    fb_deferred_io_cleanup(info);
    __free_pages(__va(info->fix.smem_start), get_order(info->fix.smem_len));

    // printk(KERN_INFO "oled remove success, close oled\n");
    return 0;
}

static const struct i2c_device_id oled_id_table[] = {
    {DEVTREE_NODE_COMPAT_INFO, 0},
};
static const struct of_device_id oled_of_match_table[] = {
    {
        .compatible = DEVTREE_NODE_COMPAT_INFO,
    },
};

// i2c_add_driver() 使用
static struct i2c_driver g_oled_driver_st = {
    .probe = oled_probe,
    .remove = oled_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "oled",
        .of_match_table = oled_of_match_table,
    },
    .id_table = oled_id_table,
};

static int oledfb_update_bl(struct backlight_device *bdev)
{
	oled_0_96_device_t *par = bl_get_data(bdev);
	int ret;
	int brightness = bdev->props.brightness;

	par->contrast = brightness;

	ret = oled_sendcmd(par->client, 0x81);
	if (ret < 0)
		return ret;
	ret = oled_sendcmd(par->client, par->contrast);
	if (ret < 0)
		return ret;
	return 0;
}

static int oledfb_get_brightness(struct backlight_device *bdev)
{
	oled_0_96_device_t *par = bl_get_data(bdev);

	return par->contrast;
}

static int oledfb_check_fb(struct backlight_device *bdev, struct fb_info *info)
{
	return (info->bl_dev == bdev);
}

static ssize_t oled_fb_write(struct fb_info *info, const char __user *buf, size_t count, loff_t *ppos)
{
    oled_0_96_device_t *par = info->par;
    unsigned long total_size;
    unsigned long p = *ppos;
    u8 __iomem *dst;

    total_size = info->fix.smem_len;

    if (p > total_size)
        return -EINVAL;

    if (count + p > total_size)
        count = total_size - p;

    if (!count)
        return -EINVAL;

    dst = (void __force *)(info->screen_base + p);

    if (copy_from_user(dst, buf, count))
        return -EFAULT;

    oled_update_display(par);

    *ppos += count;

    // printk(KERN_INFO "oled_fb_write()\n");
    return count;
}

static void oled_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
    oled_0_96_device_t *par = info->par;
    sys_fillrect(info, rect);
    oled_update_display(par);
    // printk(KERN_INFO "oled_fb_fillrect()\n");
}

static void oled_fb_copyarea(struct fb_info *info, const struct fb_copyarea *region)
{
    oled_0_96_device_t *par = info->par;
    sys_copyarea(info, region);
    oled_update_display(par);
    // printk(KERN_INFO "oled_fb_copyarea()\n");
}

static void oled_fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
    oled_0_96_device_t *par = info->par;
    sys_imageblit(info, image);
    oled_update_display(par);
    // printk(KERN_INFO "oled_fb_imageblit()\n");
}

static int oled_fb_blank(int blank_mode, struct fb_info *info)
{
    oled_0_96_device_t *par = info->par;

    if (blank_mode != FB_BLANK_UNBLANK)
        return oled_sendcmd(par->client, OLED_CMD_DISPLAY_OFF);
    else
        return oled_sendcmd(par->client, OLED_CMD_DISPLAY_ON);
    // printk(KERN_INFO "oled_fb_blank()\n");
    return 0;
}

// oled 的相关操作函数
static struct fb_ops oled_fbops = {
    .owner = THIS_MODULE,
    .fb_read = fb_sys_read,
    .fb_write = oled_fb_write,
    .fb_blank = oled_fb_blank,
    .fb_fillrect = oled_fb_fillrect,
    .fb_copyarea = oled_fb_copyarea,
    .fb_imageblit = oled_fb_imageblit, // 必须实现否则段错误
};

/**
 * @name: oled_init
 * @msg: 模块入口函数
 * @return {0} success
 * @return {1} fail, led_gpio_init() error
 * @return {2} fail, led_dev_init() error
 * @author: TOTHTOT
 * @Date: 2023-09-28 14:59:30
 */
static int32_t __init oled_init(void)
{
    int32_t ret = 0;

    // printk(KERN_INFO "oled init start\n");
    ret = i2c_add_driver(&g_oled_driver_st);
    if (ret != 0)
    {
        printk(KERN_ERR "i2c_add_driver() error\n");
        return 1;
    }
    // printk(KERN_INFO "oled init success\n");
    return 0;
}

/**
 * @name: oled_exit
 * @msg: 模块出口函数
 * @return {*}
 * @author: TOTHTOT
 * @date: 2023年8月19日
 */
static void __exit oled_exit(void)
{
    // printk(KERN_INFO "oled exit start\n");
    // 删除 i2c 设备, 会调用 oled_remove()
    i2c_del_driver(&g_oled_driver_st);
    // 注销framebuffer
    // unregister_framebuffer(&g_oled_info_st);

    device_destroy(g_oled_0_96_dev_st.class, g_oled_0_96_dev_st.devid);
    class_destroy(g_oled_0_96_dev_st.class);
    cdev_del(&g_oled_0_96_dev_st.cdev);
    unregister_chrdev_region(g_oled_0_96_dev_st.devid, DEVICE_DEV_NUM);

    // printk(KERN_INFO "oled exit success\n");
}

module_init(oled_init);
module_exit(oled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TOTHTOT");
MODULE_INFO(intree, "Y");
