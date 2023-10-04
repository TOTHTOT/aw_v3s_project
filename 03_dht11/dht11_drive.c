/*
 * @Description:
 * @Author: TOTHTOT
 * @Date: 2023-09-29 11:47:00
 * @LastEditTime: 2023-10-04 21:07:17
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
#define DEVICE_NAME "dht11"               // 显示到 /dev下的设备名字
#define DEVTREE_NODE_NAME "/dht11"        // 设备树中节点名称
#define DEVTREE_NODE_IO_INFO "gpio_dht11" // 设备树中节点io info
#define DEVTREE_NODE_IO_REQUST_NAME "dht11" // 设备树中节点io requst nema

#define DEVTREE_NODE_STATUS "status"             // 设备树中节点status
#define DEVTREE_NODE_STATUS_OK "okay"            // 设备树中节点status == okay
#define DEVTREE_NODE_COMPAT "compatible"         // 设备树中节点compatible
#define DEVTREE_NODE_COMPAT_INFO "tothtot,dht11" // 设备树中节点compatible

/* 类型定义 */
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
    int dht11_dq_gpio_num;
} dht11_drive_t;

/* 函数声明 */
static int dht11_probe(struct platform_device *pdev);
static int dht11_remove(struct platform_device *pdev);

/* 变量声明 */
static struct file_operations g_dht11_fops_st;

/* 变量定义 */
dht11_drive_t g_dht11_dev_st = {0};

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
    gpio_set_value(p_dev_st->dht11_dq_gpio_num, 0);
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
    
    if(dht11_dev_init(&g_dht11_dev_st, &g_dht11_fops_st) != 0)
    {
        printk(KERN_ERR "dev_init() fail\n");
        return 1;
    }
    if( dht11_gpio_init(&g_dht11_dev_st) != 0)
    {
        printk(KERN_ERR "gpio_init() fail\n");
        return 2;
    }

    dev_info(&pdev->dev, "initialized (%s)\n", "11");

    return 0;
}

static int dht11_remove(struct platform_device *pdev)
{
    // 在这里进行 DHT11 温湿度传感器的关闭操作
    dht11_dev_exit(&g_dht11_dev_st);
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
