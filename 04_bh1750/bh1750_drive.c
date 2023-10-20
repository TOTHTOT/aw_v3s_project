/*
 * @Description: bh1750 驱动, addr引脚接地 地址0x23 iic通信速率400khz
 * @Author: TOTHTOT
 * @Date: 2023-09-29 11:47:00
 * @LastEditTime: 2023-10-17 16:21:20
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/04_bh1750/bh1750_drive.c
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
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/i2c.h>

#define DEVICE_DEV_NUM 1
#define DEVICE_NAME "bh1750" // 显示到 /dev下的设备名字
// #define DEVTREE_NODE_NAME "/bh1750"          // 设备树中节点名称
// #define DEVTREE_NODE_IO_INFO "bh1750-i2c"    // 设备树中节点io info
#define DEVTREE_NODE_IO_REQUST_NAME "bh1750" // 设备树中节点io requst nema

#define DEVTREE_NODE_STATUS "status"           // 设备树中节点status
#define DEVTREE_NODE_STATUS_OK "okay"          // 设备树中节点status == okay
#define DEVTREE_NODE_COMPAT "compatible"       // 设备树中节点compatible
#define DEVTREE_NODE_COMPAT_INFO "rohm,bh1750" // 设备树中节点compatible

/* 函数重定义 */
#define delay_us(x) udelay(x)
#define delay_ms(x) msleep(x)

/* 传感器指令 */
#define BH1750_COMMAND_POWER_DOWN 0x00                      /**< power down command */
#define BH1750_COMMAND_POWER_ON 0x01                        /**< power on command */
#define BH1750_COMMAND_RESET 0x07                           /**< reset command */
#define BH1750_COMMAND_CONTINUOUSLY_H_RESOLUTION_MODE 0x10  /**< continuously h-resolution mode command */
#define BH1750_COMMAND_CONTINUOUSLY_H_RESOLUTION_MODE2 0x11 /**< continuously h-resolution mode2 command */
#define BH1750_COMMAND_CONTINUOUSLY_L_RESOLUTION_MODE 0x13  /**< continuously l-resolution mode command */
#define BH1750_COMMAND_ONE_TIME_H_RESOLUTION_MODE 0x20      /**< one time h-resolution mode command */
#define BH1750_COMMAND_ONE_TIME_H_RESOLUTION_MODE2 0x21     /**< one time h-resolution mode2 command */
#define BH1750_COMMAND_ONE_TIME_L_RESOLUTION_MODE 0x23      /**< one time l-resolution mode command */
#define BH1750_COMMAND_CHANGE_MEASUREMENT_TIME_HIGH 0x40    /**< change measurement time high command */
#define BH1750_COMMAND_CHANGE_MEASUREMENT_TIME_LOW 0x60     /**< change measurement time low command */

/* 常量定义 */

/* 类型定义 */
typedef struct bh1750_data
{
    // data
    uint16_t lux;
} bh1750_data_t;

typedef struct bh1750_drive
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;    // dev num
    int minor;    // dev num
    int gpio_num; // 申请道德gpio设备号

    struct i2c_client *client; // iic 相关

    bh1750_data_t sensor_data_st; // bh1750 数据
} bh1750_drive_t;

/* 函数声明 */
static int bh1750_probe(struct i2c_client *, const struct i2c_device_id *);
static int bh1750_remove(struct i2c_client *client);

/* 变量声明 */
static struct file_operations g_bh1750_fops_st;

/* 变量定义 */
bh1750_drive_t g_bh1750_dev_st = {0};

static inline int bh1750_send_cmd(struct i2c_client *client, uint8_t cmd)
{
    return i2c_master_send(client, &cmd, 1);
}

static inline int bh1750_read_dat(struct i2c_client *client, uint8_t *read_buf, int read_len)
{
    return i2c_master_recv(client, read_buf, read_len);
}

/**
 * @name: bh1750_open
 * @msg: 打开设备 传递私有数据
 * @param {inode} *inode
 * @param {file} *p_file
 * @return {0} 成功
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:30:55
 */
static int bh1750_open(struct inode *inode, struct file *p_file)
{
    p_file->private_data = &g_bh1750_dev_st;
    printk(KERN_INFO "%s open\n", DEVICE_NAME);
    return 0;
}
/**
 * @name: bh1750_read
 * @msg: 读取 bh1750 数据, 使用连续读取避免每次读取都要开机
 * @param {file} *p_file
 * @param {char __user} *user
 * @param {size_t} bytesize
 * @param {loff_t} *this_loff_t
 * @return {> 0} 成功
 * @return {< 0} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:31:26
 */
static ssize_t bh1750_read(struct file *p_file, char __user *user, size_t bytesize, loff_t *this_loff_t)
{
    bh1750_drive_t *p_dev_st = p_file->private_data;
    int32_t ret = 0;
    uint8_t read_buf[2];

    bh1750_send_cmd(p_dev_st->client, BH1750_COMMAND_CONTINUOUSLY_H_RESOLUTION_MODE2);
    bh1750_read_dat(p_dev_st->client, read_buf, sizeof(read_buf));

    p_dev_st->sensor_data_st.lux = ((uint16_t)(read_buf[0])) << 8 | read_buf[1];
    // printk(KERN_INFO "dat = %d\n", p_dev_st->sensor_data_st.lux);

    if (copy_to_user(user, &p_dev_st->sensor_data_st, sizeof(bh1750_data_t)))
    {
        printk(KERN_ERR "%s:copy_to_user() fail\n", DEVICE_NAME);
        return -2;
    }
    return ret;
}

/**
 * @name: bh1750_release
 * @msg: 释放设备
 * @param {inode} *p_inode
 * @param {file} *p_file
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:32:15
 */
static int32_t bh1750_release(struct inode *p_inode, struct file *p_file)
{
    // bh1750_drive_t *p_dev_st = p_file->private_data;

    p_file->private_data = NULL;

    printk(KERN_INFO "%s released\n", DEVICE_NAME);
    return 0;
}
static struct file_operations g_bh1750_fops_st = {
    .owner = THIS_MODULE,
    .read = bh1750_read,
    .open = bh1750_open,
    .release = bh1750_release,
};

/**
 * @name: bh1750_arg_init
 * @msg: 初始化 bh1750 的寄存器
 * @param {bh1750_drive_t} *p_dev_st
 * @return {0} 成功
 * @return {其他} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:43:25
 */
static int32_t bh1750_arg_init(bh1750_drive_t *p_dev_st)
{
    int32_t ret = 0;
    int8_t read_buf[2];
    uint16_t temp;

    ret = bh1750_send_cmd(p_dev_st->client, BH1750_COMMAND_POWER_ON);
    ret = bh1750_send_cmd(p_dev_st->client, BH1750_COMMAND_CONTINUOUSLY_H_RESOLUTION_MODE2);
    ret = bh1750_read_dat(p_dev_st->client, read_buf, 2);

    temp = ((uint16_t)(read_buf[0])) << 8 | read_buf[1];
    // temp = (float)temp/1.2 ;

    printk(KERN_INFO "light = %d lux\n", temp);
    return ret;
}

/**
 * @name: bh1750_dev_init
 * @msg: 注册 bh1750 设备到内核
 * @param {bh1750_0_96_device_t} *p_dev_st device
 * @param {file_operations} *fops_p
 * @return {0} success
 * @author: TOTHTOT
 * @Date: 2023-09-30 11:41:39
 */
static int32_t bh1750_dev_init(bh1750_drive_t *p_dev_st, struct file_operations *fops_p)
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
    printk(KERN_ERR "major = %d, minor = %d\n", p_dev_st->major, p_dev_st->minor);

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
    gpio_free(p_dev_st->gpio_num);
    return -EIO;
}
/**
 * @name: bh1750_dev_exit
 * @msg: 注销 bh1750 设备
 * @param {bh1750_drive_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:27:00
 */
static int32_t bh1750_dev_exit(bh1750_drive_t *p_dev_st)
{

    device_destroy(p_dev_st->class, p_dev_st->devid);
    class_destroy(p_dev_st->class);
    cdev_del(&p_dev_st->cdev);
    unregister_chrdev_region(p_dev_st->devid, DEVICE_DEV_NUM);

    return 0;
}

/**
 * @name: bh1750_probe
 * @msg: 设备树中匹配到 compatible 后执行
 * @param {i2c_client} *
 * @param {i2c_device_id} *
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-12 11:26:49
 */
static int bh1750_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    bh1750_drive_t *p_dev_st = NULL;

    g_bh1750_dev_st.client = client;
    i2c_set_clientdata(client, &g_bh1750_dev_st);
    p_dev_st = i2c_get_clientdata(client);

    if (bh1750_dev_init(p_dev_st, &g_bh1750_fops_st) != 0)
    {
        printk(KERN_ERR "dev_init() fail\n");
        return 1;
    }
    if(bh1750_arg_init(p_dev_st) != 0)
    {
        printk(KERN_ERR "arg_init() fail\n");
        bh1750_dev_exit(p_dev_st);
        return 2;
    }
    printk(KERN_INFO "%s probed\n", DEVICE_NAME);

    return 0;
}

/**
 * @name: bh1750_remove
 * @msg:
 * @param {i2c_client} *client
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-12 11:36:15
 */
static int bh1750_remove(struct i2c_client *client)
{
    bh1750_drive_t *p_dev_st = i2c_get_clientdata(client);
    // 在这里进行 DHT11 温湿度传感器的关闭操作
    bh1750_dev_exit(p_dev_st);

    printk(KERN_INFO "%s removed\n", DEVICE_NAME);

    return 0;
}

// 初始化drive
static const struct i2c_device_id bh1750_id_table[] = {
    {DEVTREE_NODE_COMPAT_INFO, 0},
};
static const struct of_device_id bh1750_of_match_table[] = {
    {
        .compatible = DEVTREE_NODE_COMPAT_INFO,
    },
};
static struct i2c_driver g_bh1750_i2c_driver_st = {
    .driver = {
        .owner = THIS_MODULE,
        .name = DEVICE_NAME,
        .of_match_table = bh1750_of_match_table,
    },
    .id_table = bh1750_id_table,
    .probe = bh1750_probe,
    .remove = bh1750_remove,
};

/**
 * @name: bh1750_init
 * @msg: 设备入口
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:45:35
 */
static int __init bh1750_init(void)
{
    return i2c_add_driver(&g_bh1750_i2c_driver_st);
}

/**
 * @name: bh1750_exit
 * @msg:
 * @return {*设备出口}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:45:44
 */
static void __exit bh1750_exit(void)
{
    printk(KERN_INFO "%s remove start\n", DEVICE_NAME);
    i2c_del_driver(&g_bh1750_i2c_driver_st);
}

module_init(bh1750_init);
module_exit(bh1750_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TOTHTOT");
MODULE_INFO(intree, "Y");
