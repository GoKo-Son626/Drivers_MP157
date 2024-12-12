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
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TEMP_CNT       1
#define TEMP_NAME        "mp1-temp"

/* gpioled设备结构体 */
struct temp_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
};

struct temp_dev tempdev;	        /* led设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 			一般在open的时候将private_data指向设备结构体。
 * @return 		: 0 成功;其他 失败
 */
static int temp_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &tempdev; /* 设置私有数据 */
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
static ssize_t temp_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t temp_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;

	return retvalue;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 		: 0 成功;其他 失败
 */
static int temp_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations temp_fops = {
	.owner = THIS_MODULE,
	.open = temp_open,
	.read = temp_read,
	.write = temp_write,
	.release = temp_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init temp_init(void)
{
	int ret = 0;

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (tempdev.major) {		/*  定义了设备号 */
		tempdev.devid = MKDEV(tempdev.major, 0);
		ret = register_chrdev_region(tempdev.devid, TEMP_CNT, TEMP_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", TEMP_NAME, TEMP_CNT);
			return ret;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&tempdev.devid, 0, TEMP_CNT, TEMP_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", TEMP_NAME, ret);
			return ret;
		}
		tempdev.major = MAJOR(tempdev.devid);	/* 获取分配号的主设备号 */
		tempdev.minor = MINOR(tempdev.devid);	/* 获取分配号的次设备号 */
	}
	printk("tempdev major=%d,minor=%d\r\n",tempdev.major, tempdev.minor);	
	
	/* 2、初始化cdev */
	tempdev.cdev.owner = THIS_MODULE;
	cdev_init(&tempdev.cdev, &temp_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&tempdev.cdev, tempdev.devid, TEMP_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	tempdev.class = class_create(THIS_MODULE, TEMP_NAME);
	if (IS_ERR(tempdev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	tempdev.device = device_create(tempdev.class, NULL, tempdev.devid, NULL, TEMP_NAME);
	if (IS_ERR(tempdev.device)) {
		goto destroy_class;
	}
	return 0;
	
destroy_class:
	class_destroy(tempdev.class);
del_cdev:
	cdev_del(&tempdev.cdev);
del_unregister:
	unregister_chrdev_region(tempdev.devid, TEMP_CNT);
	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit temp_exit(void)
{
	/* 注销字符设备驱动 */
	cdev_del(&tempdev.cdev);/*  删除cdev */
	unregister_chrdev_region(tempdev.devid, TEMP_CNT); /* 注销设备号 */
	device_destroy(tempdev.class, tempdev.devid);/* 注销设备 */
	class_destroy(tempdev.class);/* 注销类 */
}

module_init(temp_init);
module_exit(temp_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GOKU-SON");
MODULE_INFO(intree, "Y");