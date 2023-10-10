/*
 * @Description: dht11 驱动
 * @Author: TOTHTOT
 * @Date: 2023-09-29 11:47:00
 * @LastEditTime: 2023-10-10 10:17:04
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/03_dht11/dht11_drive.c
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

#define DEVICE_DEV_NUM 1
#define DEVICE_NAME "dht11"                 // 显示到 /dev下的设备名字
#define DEVTREE_NODE_NAME "/dht11"          // 设备树中节点名称
#define DEVTREE_NODE_IO_INFO "gpio_dht11"   // 设备树中节点io info
#define DEVTREE_NODE_IO_REQUST_NAME "dht11" // 设备树中节点io requst nema

#define DEVTREE_NODE_STATUS "status"             // 设备树中节点status
#define DEVTREE_NODE_STATUS_OK "okay"            // 设备树中节点status == okay
#define DEVTREE_NODE_COMPAT "compatible"         // 设备树中节点compatible
#define DEVTREE_NODE_COMPAT_INFO "tothtot,dht11" // 设备树中节点compatible

/* 函数重定义 */
#define delay_us(x) udelay(x)
#define delay_ms(x) msleep(x)

/* 常量定义 */
#define DHT11_RETRY_CNT 10000 // 延时 1us 情况下

/* 类型定义 */
typedef struct dht11_data
{
    // data
    uint16_t temp;
    uint16_t humi;
} dht11_data_t;

typedef struct dht11_drive
{
    int dht11_fd;
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major; // dev num
    int minor; // dev num

    struct device_node *nd; // dev tree node info
    int32_t dht11_dq_gpio_num;

    dht11_data_t sensor_data_st; // dht11 数据
} dht11_drive_t;

/* 函数声明 */
static int dht11_probe(struct platform_device *pdev);
static int dht11_remove(struct platform_device *pdev);

/* 变量声明 */
static struct file_operations g_dht11_fops_st;

/* 变量定义 */
dht11_drive_t g_dht11_dev_st = {0};

/**
 * @name: dht11_read_io
 * @msg: 读取io电平,会设置成输入
 * @param {int32_t} io_num 申请到的 gpio 号
 * @return {0/1} 读取到的io状态
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:22:16
 */
static inline uint8_t dht11_read_io(int32_t io_num)
{
    uint8_t io_state = 0;

    gpio_direction_input(io_num);
    io_state = gpio_get_value(io_num);
    return io_state;
}

/**
 * @name: dht11_rst
 * @msg: dht11 复位
 * @param {int32_t} io_num
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:29:06
 */
static uint8_t dht11_rst(int32_t io_num)
{
    gpio_direction_output(io_num, 0);
    delay_ms(20);
    gpio_direction_output(io_num, 1);
    delay_us(40);

    return 0;
}

/**
 * @name: dht11_check
 * @msg: 检测 dht11 是否在线
 * @param {int32_t} io_num
 * @return {0} 在线
 * @return {1} 掉线
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:36:54
 */
static uint8_t dht11_check(int32_t io_num)
{
    uint16_t retry_cnt_cnt = 0;

    while (dht11_read_io(io_num) == 1 && retry_cnt_cnt < DHT11_RETRY_CNT)
    {
        retry_cnt_cnt++;
        delay_us(1);
    }
    if (retry_cnt_cnt >= DHT11_RETRY_CNT)
    {
        return 1;
    }

    retry_cnt_cnt = 0;
    while (dht11_read_io(io_num) == 0 && retry_cnt_cnt < DHT11_RETRY_CNT)
    {
        retry_cnt_cnt++;
        delay_us(1);
    }
    if (retry_cnt_cnt >= DHT11_RETRY_CNT)
    {
        return 2;
    }
    else
    {
        return 0;
    }
}

/**
 * @name: dht11_read_bit
 * @msg: 读取1bit数据
 * @param {int32_t} io_num
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:47:41
 */
static uint8_t dht11_read_bit(int32_t io_num)
{
    uint8_t retry_cnt = 0;
    while ((dht11_read_io(io_num) == 1) && retry_cnt < DHT11_RETRY_CNT) // 等待变为低电平
    {
        retry_cnt++;
        delay_us(1);
    }
    retry_cnt = 0;
    while ((dht11_read_io(io_num) == 0) && retry_cnt < DHT11_RETRY_CNT) // 等待变高电平
    {
        retry_cnt++;
        delay_us(1);
    }
    delay_us(40); // 等待40us
    if (dht11_read_io(io_num) == 1)
        return 1;
    else
        return 0;
}

/**read() fail[-1]

 * @msg: 读取1字节
 * @param {int32_t} io_num
 * @return {读取到的数据}
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:48:58
 */
static uint8_t dht11_read_byte(int32_t io_num)
{
    uint8_t i, dat;
    dat = 0;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        dat |= dht11_read_bit(io_num); // 读取一个位
    }
    return dat;
}

/**
 * @name: dht11_read_data
 * @msg: dht11 读取数据
 * @param {int32_t} io_num gpio号
 * @param {u_int16_t} *temp 温度
 * @param {u_int16_t} *humi 湿度
 * @return {0} 成功
 * @return {1} 失败
 * @return {2} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-05 09:59:58
 */
static uint8_t dht11_read_data(int32_t io_num, u_int16_t *temp, u_int16_t *humi)
{
    uint8_t buf[5];
    uint8_t i, ret = 0;

    dht11_rst(io_num);

    ret = dht11_check(io_num);
    if (ret == 0)
    {
        for (i = 0; i < 5; i++) // 读取40位数据
        {
            buf[i] = dht11_read_byte(io_num); // 读取一个数据
        }
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) // 校验
        {
            *humi = buf[0] * 10 + buf[1];
            *temp = buf[2] * 10 + buf[3];
            // printk(KERN_INFO "dht11 readout data:buf=%d,%d,%d,%d,%d\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            // printk(KERN_INFO "dht11 temp = %d, humi = %d\n", *temp, *humi);
        }
        else
        {
            // printk(KERN_ERR "dht11 check data error[buf=%d,%d,%d,%d,%d]\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
            return 3;
        }
    }
    else
        return ret;

    return 0;
}

/**
 * @name: dht11_open
 * @msg: 打开设备 传递私有数据
 * @param {inode} *inode
 * @param {file} *p_file
 * @return {0} 成功
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:30:55
 */
static int dht11_open(struct inode *inode, struct file *p_file)
{
    p_file->private_data = &g_dht11_dev_st;
    printk(KERN_INFO "%s open\n", DEVICE_NAME);
    return 0;
}
/**
 * @name: dht11_read
 * @msg: 读取 dht11 数据
 * @param {file} *p_file
 * @param {char __user} *user
 * @param {size_t} bytesize
 * @param {loff_t} *this_loff_t
 * @return {> 0} 成功
 * @return {< 0} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:31:26
 */
static ssize_t dht11_read(struct file *p_file, char __user *user, size_t bytesize, loff_t *this_loff_t)
{
    dht11_drive_t *p_dev_st = p_file->private_data;
    int32_t ret = 0;

    p_dev_st->sensor_data_st.temp = 0;
    p_dev_st->sensor_data_st.humi = 0;

    ret = dht11_read_data(p_dev_st->dht11_dq_gpio_num, &p_dev_st->sensor_data_st.temp, &p_dev_st->sensor_data_st.humi);
    gpio_direction_output(p_dev_st->dht11_dq_gpio_num, 1);
    if (ret == 0)
    {
        // 将温度和湿度信息拷贝到user指针中
        if (copy_to_user(user, &p_dev_st->sensor_data_st, sizeof(dht11_data_t)))
        {
            printk(KERN_ERR "%s:copy_to_user() fail\n", DEVICE_NAME);
            return -2;
        }
        else
        {
            // printk(KERN_INFO "%s:dht11_read_data() success\n", DEVICE_NAME);
            return sizeof(dht11_data_t);
        }
    }
    else
    {
        if (copy_to_user(user, &p_dev_st->sensor_data_st, sizeof(dht11_data_t)))
        {
            // printk(KERN_ERR "%s:copy_to_user() fail\n", DEVICE_NAME);
            return -2;
        }
        printk(KERN_ERR "%s:dht11_read_data() fail[%d]\n", DEVICE_NAME, ret);
        return -1;
    }
}

/**
 * @name: dht11_release
 * @msg: 释放设备
 * @param {inode} *p_inode
 * @param {file} *p_file
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-05 10:32:15
 */
static int32_t dht11_release(struct inode *p_inode, struct file *p_file)
{
    dht11_drive_t *p_dev_st = p_file->private_data;

    gpio_direction_output(p_dev_st->dht11_dq_gpio_num, 1); // io口拉高
    p_file->private_data = NULL;

    printk(KERN_INFO "%s released\n", DEVICE_NAME);
    return 0;
}
static struct file_operations g_dht11_fops_st = {
    .owner = THIS_MODULE,
    .read = dht11_read,
    .open = dht11_open,
    .release = dht11_release,
};

/**
 * @name: dht11_gpio_init
 * @msg: 从设备树获取 gpio 信息
 * @param {dht11_drive_t} *p_dev_st
 * @return {0} 成功
 * @return {其他} 失败
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:43:25
 */
static int32_t dht11_gpio_init(dht11_drive_t *p_dev_st)
{
    int32_t ret = 0;
    const char *str;

    /* 从设备树中获取led的IO口信息 */
    p_dev_st->nd = of_find_node_by_path(DEVTREE_NODE_NAME);
    if (p_dev_st->nd == NULL)
    {
        printk(KERN_ERR "gpioled node not find\n");
        return -EINVAL;
    }
    ret = of_property_read_string(p_dev_st->nd, DEVTREE_NODE_STATUS, &str);
    if (ret < 0)
    {
        printk(KERN_ERR "read status error\n");
        return -EINVAL;
    }
    if (strcmp(str, DEVTREE_NODE_STATUS_OK) != 0)
        return -EINVAL;
    ret = of_property_read_string(p_dev_st->nd, DEVTREE_NODE_COMPAT, &str);
    if (ret < 0)
    {
        printk(KERN_ERR "read status error\n");
        return -EINVAL;
    }
    if (strcmp(str, DEVTREE_NODE_COMPAT_INFO) != 0)
        return -EINVAL;

    // 获取IO口属性
    p_dev_st->dht11_dq_gpio_num = of_get_named_gpio(p_dev_st->nd, DEVTREE_NODE_IO_INFO, 0);
    if (p_dev_st->dht11_dq_gpio_num < 0)
    {
        printk(KERN_ERR "get dht11 failed\n");
        return -EINVAL;
    }

    printk(KERN_ERR "gpio_requst_num = %d\n", p_dev_st->dht11_dq_gpio_num);

    ret = gpio_request(p_dev_st->dht11_dq_gpio_num, DEVTREE_NODE_IO_REQUST_NAME);
    if (ret < 0)
    {
        printk(KERN_ERR "gpio_request failed\n");
        return ret;
    }

    // 设置为输出,且为物理电平为高电平
    ret = gpio_direction_output(p_dev_st->dht11_dq_gpio_num, 1);
    if (ret < 0)
    {
        printk(KERN_ERR "set gpio direction error\n");
        return -EINVAL;
    }
    return 0;
}

/**
 * @name: dht11_dev_init
 * @msg: 注册 dht11 设备到内核
 * @param {dht11_0_96_device_t} *p_dev_st device
 * @param {file_operations} *fops_p
 * @return {0} success
 * @author: TOTHTOT
 * @Date: 2023-09-30 11:41:39
 */
static int32_t dht11_dev_init(dht11_drive_t *p_dev_st, struct file_operations *fops_p)
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
    gpio_free(p_dev_st->dht11_dq_gpio_num);
    return -EIO;
}
/**
 * @name: dht11_dev_exit
 * @msg: 注销 dht11 设备
 * @param {dht11_drive_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:27:00
 */
static int32_t dht11_dev_exit(dht11_drive_t *p_dev_st)
{
    gpio_direction_output(p_dev_st->dht11_dq_gpio_num, 1);
    device_destroy(p_dev_st->class, p_dev_st->devid);
    class_destroy(p_dev_st->class);
    cdev_del(&p_dev_st->cdev);
    unregister_chrdev_region(p_dev_st->devid, DEVICE_DEV_NUM);

    gpio_free(p_dev_st->dht11_dq_gpio_num);

    return 0;
}

/**
 * @name: dht11_probe
 * @msg: 设备树中匹配到 compatible 后执行
 * @param {platform_device} *pdev
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:45:00
 */
static int dht11_probe(struct platform_device *pdev)
{
    platform_set_drvdata(pdev, &g_dht11_dev_st);

    if (dht11_dev_init(&g_dht11_dev_st, &g_dht11_fops_st) != 0)
    {
        printk(KERN_ERR "dev_init() fail\n");
        return 1;
    }
    if (dht11_gpio_init(&g_dht11_dev_st) != 0)
    {
        printk(KERN_ERR "gpio_init() fail\n");
        return 2;
    }

    dev_info(&pdev->dev, "initialized (%s)\n", DEVICE_NAME);

    return 0;
}

static int dht11_remove(struct platform_device *pdev)
{
    dht11_drive_t *p_dev_st = platform_get_drvdata(pdev);
    // 在这里进行 DHT11 温湿度传感器的关闭操作
    dht11_dev_exit(p_dev_st);
    dev_info(&pdev->dev, "removed\n");

    return 0;
}

static const struct of_device_id dht11_of_match[] = {
    {.compatible = DEVTREE_NODE_COMPAT_INFO},
    {},
};
MODULE_DEVICE_TABLE(of, dht11_of_match);

static struct platform_driver dht11_platform_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .of_match_table = dht11_of_match,
        .owner = THIS_MODULE,
    },
    .probe = dht11_probe,
    .remove = dht11_remove,
};

/**
 * @name: dht11_init
 * @msg: 设备入口
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:45:35
 */
static int __init dht11_init(void)
{
    return platform_driver_register(&dht11_platform_driver);
}

/**
 * @name: dht11_exit
 * @msg:
 * @return {*设备出口}
 * @author: TOTHTOT
 * @Date: 2023-10-04 20:45:44
 */
static void __exit dht11_exit(void)
{
    platform_driver_unregister(&dht11_platform_driver);
}

module_init(dht11_init);
module_exit(dht11_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TOTHTOT");
MODULE_INFO(intree, "Y");
