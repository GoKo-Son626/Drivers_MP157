/*
 * @Date: 2024-12-23
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-24
 * @FilePath: /1-STM32MP157/Drivers_MP157/15_MISC-beep/misc_beep.c
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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MISCBEEP_NAME		"miscbeep"	/* 名字 	*/
#define MISCBEEP_MINOR		144			/* 子设备号 */
#define BEEPOFF 			0			/* 关蜂鸣器 */
#define BEEPON 				1			/* 开蜂鸣器 */

/* miscbeep设备结构体 */
struct miscbeep_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	/* 类 		*/
	struct device *device;	/* 设备 	 */
	int beep_gpio;			/* beep所使用的GPIO编号		*/
};

// 主要用于beep_gpio_init
struct miscbeep_dev miscbeep;		/* beep设备 */

/* beep相关初始化 */
static int beep_gpio_init(struct device_node *nd)
{
	int ret;
	
	/* 从设备树中获取GPIO */
	miscbeep.beep_gpio = of_get_named_gpio(nd, "beep-gpio", 0);
	if(!gpio_is_valid(miscbeep.beep_gpio)) {
		printk("miscbeep：Failed to get beep-gpio\n");
		return -EINVAL;
	}
	
	/* 申请使用GPIO */
	ret = gpio_request(miscbeep.beep_gpio, "beep");
	if(ret) {
		printk("beep: Failed to request beep-gpio\n");
		return ret;
	}
	
	/* 将GPIO设置为输出模式并设置GPIO初始化电平状态 */
	gpio_direction_output(miscbeep.beep_gpio, 1);
	
	return 0;
}

static int miscbeep_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t miscbeep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char beepstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	beepstat = databuf[0];		/* 获取状态值 */
	if(beepstat == BEEPON) {	
		gpio_set_value(miscbeep.beep_gpio, 0);	/* 打开蜂鸣器 */
	} else if(beepstat == BEEPOFF) {
		gpio_set_value(miscbeep.beep_gpio, 1);	/* 关闭蜂鸣器 */
	}
	return 0;
}

/* 设备操作函数 */
static struct file_operations miscbeep_fops = {
	.owner = THIS_MODULE,
	.open = miscbeep_open,
	.write = miscbeep_write,
};

/* MISC设备结构体 */
static struct miscdevice beep_miscdev = {
	.minor = MISCBEEP_MINOR,
	.name = MISCBEEP_NAME,
	.fops = &miscbeep_fops,
};

 /*
  * @description     : flatform驱动的probe函数，当驱动与
  *                    设备匹配以后此函数就会执行
  * @param - dev     : platform设备
  * @return          : 0，成功;其他负值,失败
  */
static int miscbeep_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk("beep driver and device was matched!\r\n");

	/* 初始化BEEP */
	ret = beep_gpio_init(pdev->dev.of_node);
	if(ret < 0)
		return ret;
		
	/* 一般情况下会注册对应的字符设备，但是这里我们使用MISC设备
  	 * 所以我们不需要自己注册字符设备驱动，只需要注册misc设备驱动即可
	 */
	ret = misc_register(&beep_miscdev);
	if(ret < 0){
		printk("misc device register failed!\r\n");
		goto free_gpio;
	}

	return 0;
	
free_gpio:
	gpio_free(miscbeep.beep_gpio);
	return -EINVAL;
}

/*
 * @description     : platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @param - dev     : platform设备
 * @return          : 0，成功;其他负值,失败
 */
static int miscbeep_remove(struct platform_device *dev)
{
	/* 注销设备的时候关闭LED灯 */
	gpio_set_value(miscbeep.beep_gpio, 1);
	
	/* 释放BEEP */
	gpio_free(miscbeep.beep_gpio);

	/* 注销misc设备 */
	misc_deregister(&beep_miscdev);
	return 0;
}

 /* 匹配列表 */
 static const struct of_device_id beep_of_match[] = {
     { .compatible = "alientek,beep" },
     { /* Sentinel */ }
 };
 
 /* platform驱动结构体 */
static struct platform_driver beep_driver = {
     .driver     = {
         .name   = "stm32mp1-beep",         /* 驱动名字，用于和设备匹配 */
         .of_match_table = beep_of_match, /* 设备树匹配表          */
     },
     .probe      = miscbeep_probe,
     .remove     = miscbeep_remove,
};

static int __init miscbeep_init(void)
{
        return platform_driver_register(&beep_driver);
}

static void __exit miscbeep_exit(void)
{
        platform_driver_unregister(&beep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GOKU-SON");
MODULE_INFO(intree, "Y");










