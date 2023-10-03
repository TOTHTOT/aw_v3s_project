/*
 * @Description: gpio 方式驱动rgbled
 * @Author: TOTHTOT
 * @Date: 2023-03-01 14:24:06
 * @LastEditTime: 2023-09-30 11:34:49
 * @LastEditors: TOTHTOT
 * @FilePath: /aw_v3s_project/01_ctrl_rgbled/rgbled_drive.c
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

#define SET_BIT(reg, bit)       ((reg) |= (1 << (bit)))
#define CLEAR_BIT(reg, bit)     ((reg) &= ~(1 << (bit)))
#define TOGGLE_BIT(reg, bit)    ((reg) ^= (1 << (bit)))
#define GET_BIT(reg, bit)       (((reg) >> (bit)) & 1)


#define DEVICE_DEV_NUM 1
#define DEVICE_NAME "rgbled"    // 显示到 /dev下的设备名字
#define DEVTREE_NODE_NAME "/rgb_leds" // 设备树中节点名称
#define DEVTREE_NODE_IO_INFO "gpio_rgbleds" // 设备树中节点io info
#define DEVTREE_NODE_IO_REQUST_NAME "RGBLEDS" // 设备树中节点io requst nema
#define DEVTREE_NODE_STATUS "status" // 设备树中节点status
#define DEVTREE_NODE_STATUS_OK "okay" // 设备树中节点status == okay
#define DEVTREE_NODE_COMPAT "compatible" // 设备树中节点compatible
#define DEVTREE_NODE_COMPAT_INFO "tothtot,rgb_leds" // 设备树中节点compatible

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

// led device struct
typedef struct
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;  // dev num
    int minor;  // dev num

    struct device_node *nd; // dev tree node info
    #define RGBLEDS_IO_NUM 3
    int led_gpio_num[RGBLEDS_IO_NUM];       // 申请到的GPIO编号, blue gree red
    uint8_t led_status_bit;
} gpio_dev_t;

gpio_dev_t g_led_dev_st;

/**
 * @name: led_open
 * @msg: open函数
 * @param {inode} *inode
 * @param {file} *p_file
 * @return {*}
 * @author: TOTHTOT
 * @date:2023年8月19日
 */
static int led_open(struct inode *inode, struct file *p_file)
{
    p_file->private_data = &g_led_dev_st;
    return 0;
}

static ssize_t led_read(struct file *p_file, char __user *buf, size_t count, loff_t *offt)
{
#define LED_STATE_BUF_SIZE 1
    ssize_t ret = 0;
    gpio_dev_t *led_dev = p_file->private_data;
    int led_state = (gpio_get_value(led_dev->led_gpio_num[0]) == RGBLED_STATUS_BLUE_ON) ? 1 : 0; // 亮就返回1

    if (count > LED_STATE_BUF_SIZE)
        count = LED_STATE_BUF_SIZE;

    // printk(KERN_ERR "driver led state %d\n", led_state);
    if (copy_to_user(buf, (char *)&led_state, count) == 0)
    {
        ret = count;
    }
    else
    {
        ret = -EFAULT;
    }

    return ret;
}

/**
 * @name: led_write
 * @msg: 写入led状态
 * @param {file} *p_file
 * @param {char __user} *buf
 * @param {size_t} count
 * @param {loff_t} *offt
 * @return {0} 成功
 * @return {其他} 失败
 * @author: TOTHTOT
 * @date: 日期
 */
static int32_t led_write(struct file *p_file, const char __user *buf, size_t count, loff_t *offt)
{
    int32_t ret = 0;
    rgbled_status_t led_status;
    gpio_dev_t *led_dev = p_file->private_data;

    ret = copy_from_user(&led_status, buf, count);
    if (ret < 0)
    {
        printk(KERN_ERR "copy_from_user failed %d\n", ret);
        return -EFAULT;
    }
    // printk(KERN_ERR "write = %d\n", led_status);
    switch (led_status)
    {
    case RGBLED_STATUS_ALL_ON:
        CLEAR_BIT(led_dev->led_status_bit, 0);
        CLEAR_BIT(led_dev->led_status_bit, 1);
        CLEAR_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_ALL_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_BLUE_ON:
        CLEAR_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_BLUE_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        break;
    case RGBLED_STATUS_GREE_ON:
        SET_BIT(led_dev->led_status_bit, 0);
        CLEAR_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_GREE_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_RED_ON:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        CLEAR_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_RED_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_YELLOW_ON:
        SET_BIT(led_dev->led_status_bit, 0);
        CLEAR_BIT(led_dev->led_status_bit, 1);
        CLEAR_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_YELLOW_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_CYAN_ON:
        CLEAR_BIT(led_dev->led_status_bit, 1);
        CLEAR_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_CYAN_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_MAGENTA_ON:
        CLEAR_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        CLEAR_BIT(led_dev->led_status_bit, 2);
        break;
    case RGBLED_STATUS_MAGENTA_OFF:
        SET_BIT(led_dev->led_status_bit, 0);
        SET_BIT(led_dev->led_status_bit, 1);
        SET_BIT(led_dev->led_status_bit, 2);
        break;
    default:
        break;
    }

    gpio_set_value(led_dev->led_gpio_num[0], GET_BIT(led_dev->led_status_bit, 0));
    gpio_set_value(led_dev->led_gpio_num[1], GET_BIT(led_dev->led_status_bit, 1));
    gpio_set_value(led_dev->led_gpio_num[2], GET_BIT(led_dev->led_status_bit, 2));

    return 0;
}

/**
 * @name: led_release
 * @msg: release when close() running
 * @param {inode} *p_inode
 * @param {file} *p_file
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-28 16:46:43
 */
static int32_t led_release(struct inode *p_inode, struct file *p_file)
{
    uint32_t i = 0;
    gpio_dev_t *led_dev = p_file->private_data;

    for (i = 0; i < RGBLEDS_IO_NUM; i++)
        gpio_set_value(led_dev->led_gpio_num[i], RGBLED_STATUS_ALL_ON);

    p_file->private_data = NULL;
    printk(KERN_ERR "rgbled released\n");
    return 0;
}

static struct file_operations g_led_fops_st = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/**
 * @name: led_gpio_init
 * @msg: gpio init
 * @param {gpio_dev_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-09-28 13:32:28
 */
static int32_t led_gpio_init(gpio_dev_t *p_dev_st)
{
    int32_t ret = 0;
    const char *str;
    int i = 0;

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
    for (i = 0; i < RGBLEDS_IO_NUM; i++)
    {
        p_dev_st->led_gpio_num[i] = of_get_named_gpio(p_dev_st->nd, DEVTREE_NODE_IO_INFO, i);
        if (p_dev_st->led_gpio_num[i] < 0)
        {
            printk(KERN_ERR "get rgbled[%d] failed\n", i);
            return -EINVAL;
        }
    }

    for (i = 0; i < RGBLEDS_IO_NUM; i++)
        printk(KERN_ERR "rgbleds[%d] = %d\n", i, p_dev_st->led_gpio_num[i]);

    for (i = 0; i< RGBLEDS_IO_NUM; i++)
    {
        ret = gpio_request(p_dev_st->led_gpio_num[i], DEVTREE_NODE_IO_REQUST_NAME);
        if (ret < 0)
        {
            printk(KERN_ERR "gpio_request[%d] failed\n", i);
            return ret;
        }
    }
    // 设置为输出,且为物理电平为高电平
    for (i = 0; i< RGBLEDS_IO_NUM; i++)
    {
        ret = gpio_direction_output(p_dev_st->led_gpio_num[i], 1);
        if (ret < 0)
        {
            printk(KERN_ERR "set rgbled[%d] gpio direction error\n", i);
            return -EINVAL;
        }
    }

    return 0;
}

/**
 * @name: led_dev_init
 * @msg: init dev num
 * @param {gpio_dev_t} *p_dev_st
 * @return {*}
 * @author: TOTHTOT
 * @date:
 */
static int32_t led_dev_init(gpio_dev_t *p_dev_st)
{
    int32_t ret = 0;
    int32_t i = 0;

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
    cdev_init(&p_dev_st->cdev, &g_led_fops_st);
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
    for (i = 0; i< RGBLEDS_IO_NUM; i++)
    {
        gpio_free(p_dev_st->led_gpio_num[i]);
    }
    return -EIO;
}

/**
 * @name: led_init
 * @msg: 模块入口函数
 * @return {0} success
 * @return {1} fail, led_gpio_init() error
 * @return {2} fail, led_dev_init() error
 * @author: TOTHTOT
 * @Date: 2023-09-28 14:59:30
 */
static int32_t __init led_init(void)
{
    int32_t ret = 0;
    printk(KERN_ERR "start init rgb led\n");
    ret = led_gpio_init(&g_led_dev_st);
    if(ret != 0)
    {
        return 1;
    }
    ret = led_dev_init(&g_led_dev_st);
    if(ret != 0)
    {
        return 2;
    }
    printk(KERN_ERR "success init rgb led\n");
    
    return 0;
}

/**
 * @name: led_exit
 * @msg: 模块出口函数
 * @return {*}
 * @author: TOTHTOT
 * @date: 2023年8月19日
 */
static void __exit led_exit(void)
{
    uint32_t i = 0;

    printk(KERN_ERR "rgbled exit\n");
    
    for (i = 0; i < RGBLEDS_IO_NUM; i++)
        gpio_set_value(g_led_dev_st.led_gpio_num[i], RGBLED_STATUS_ALL_ON);
    device_destroy(g_led_dev_st.class, g_led_dev_st.devid);
    class_destroy(g_led_dev_st.class);
    cdev_del(&g_led_dev_st.cdev);
    unregister_chrdev_region(g_led_dev_st.devid, DEVICE_DEV_NUM);
    
    for (i = 0; i < RGBLEDS_IO_NUM; i++)
        gpio_free(g_led_dev_st.led_gpio_num[i]);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TOTHTOT");
MODULE_INFO(intree, "Y");
