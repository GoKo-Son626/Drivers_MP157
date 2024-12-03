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
#include <linux/jiffies.h>

#define GPIOLED_CNT       1
#define GPIOLED_NAME        "timer_dev"

/* gpioled设备结构体 */
struct timer_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        struct timer_list timer;        /* 定时器 */
};

struct timer_dev timerdev;	        /* led设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 			一般在open的时候将private_data指向设备结构体。
 * @return 		: 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &gpioled; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 		: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 		: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;

	return retvalue;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 		: 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations gpioled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init led_init(void)
{
	int ret = 0;

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (gpioled.major) {		/*  定义了设备号 */
		gpioled.devid = MKDEV(gpioled.major, 0);
		ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", GPIOLED_NAME, GPIOLED_CNT);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", GPIOLED_NAME, ret);
			goto free_gpio;
		}
		gpioled.major = MAJOR(gpioled.devid);	/* 获取分配号的主设备号 */
		gpioled.minor = MINOR(gpioled.devid);	/* 获取分配号的次设备号 */
	}
	printk("gpioled major=%d,minor=%d\r\n",gpioled.major, gpioled.minor);	
	
	/* 2、初始化cdev */
	gpioled.cdev.owner = THIS_MODULE;
	cdev_init(&gpioled.cdev, &gpioled_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
	if (IS_ERR(gpioled.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
	if (IS_ERR(gpioled.device)) {
		goto destroy_class;
	}
	return 0;
	
destroy_class:
	class_destroy(gpioled.class);
del_cdev:
	cdev_del(&gpioled.cdev);
del_unregister:
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
free_gpio:
	gpio_free(gpioled.led_gpio);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit led_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&gpioled.cdev);/*  删除cdev */
	unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); /* 注销设备号 */
	device_destroy(gpioled.class, gpioled.devid);/* 注销设备 */
	class_destroy(gpioled.class);/* 注销类 */
	gpio_free(gpioled.led_gpio); /* 释放GPIO */
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");