/*
 * @Date: 2024-12-03
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-03
 * @FilePath: /1-STM32MP157/Drivers_MP157/07_key/key.c
 * @Description: 
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
#include <linux/semaphore.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <stdatomic.h>

#define KEY0_CNT        1
#define KEY0_NAME       "KEY0"

/* 定义按键值 */
#define INVAKEY         0X00
#define key0value       0XF0

/* gpioled设备结构体 */
struct key0_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        int key0_gpio;	                /* 使用的GPIO编号 */
        atomic_t keyvalue;	        /* 按键值 */
};

struct key0_dev key0;	        /* key0设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 			一般在open的时候将private_data指向设备结构体。
 * @return 		: 0 成功;其他 失败
 */
static int key0_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &key0; /* 设置私有数据 */
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
static ssize_t key0_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t key0_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;

	return retvalue;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 		: 0 成功;其他 失败
 */
static int key0_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations key0_fops = {
	.owner = THIS_MODULE,
	.open = key0_open,
	.read = key0_read,
	.write = key0_write,
	.release = key0_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init key0_init(void)
{
	int ret = 0;

        /* 1. 初始化原子变量 */
        key0.keyvalue = (atomic_t)ATOMIC_INIT(0);

        /* 2. 原子变量初始值为INVAKEY */
        atomic_set(&key0.keyvalue, INVAKEY);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (key0.major) {		/*  定义了设备号 */
		key0.devid = MKDEV(key0.major, 0);
		ret = register_chrdev_region(key0.devid, KEY0_CNT, KEY0_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", KEY0_NAME, KEY0_CNT);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&key0.devid, 0, KEY0_CNT, KEY0_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY0_NAME, ret);
			goto free_gpio;
		}
		key0.major = MAJOR(key0.devid);	/* 获取分配号的主设备号 */
		key0.minor = MINOR(key0.devid);	/* 获取分配号的次设备号 */
	}
	printk("key0 major=%d,minor=%d\r\n",key0.major, key0.minor);	
	
	/* 2、初始化cdev */
	key0.cdev.owner = THIS_MODULE;
	cdev_init(&key0.cdev, &key0_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&key0.cdev, key0.devid, KEY0_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	key0.class = class_create(THIS_MODULE, KEY0_NAME);
	if (IS_ERR(key0.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	key0.device = device_create(key0.class, NULL, key0.devid, NULL, KEY0_NAME);
	if (IS_ERR(key0.device)) {
		goto destroy_class;
	}
	return 0;
	
destroy_class:
	class_destroy(key0.class);
del_cdev:
	cdev_del(&key0.cdev);
del_unregister:
	unregister_chrdev_region(key0.devid, KEY0_CNT);
free_gpio:
	gpio_free(key0.key0_gpio);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit key0_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&key0.cdev);/*  删除cdev */
	unregister_chrdev_region(key0.devid, KEY0_CNT); /* 注销设备号 */
	device_destroy(key0.class, key0.devid);/* 注销设备 */
	class_destroy(key0.class);/* 注销类 */
	gpio_free(key0.key0_gpio); /* 释放GPIO */
}

module_init(key0_init);
module_exit(key0_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");






































