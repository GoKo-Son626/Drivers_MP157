/*
 * @Date: 2024-12-11
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-21
 * @FilePath: /1-STM32MP157/Drivers_MP157/13_platform_device/platform_led.c
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
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LED_CNT       1
#define LED_NAME        "mp1-leddev"
#define LEDON           0
#define LEDOFF          1

/* gpioled设备结构体 */
struct led_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        
        int gpio_led;	                /* GPIO */        
};

struct led_dev leddev;	        /* led设备 */

void led_switch(u8 sta)
{
        if (sta == 0)
                gpio_set_value(leddev.gpio_led, 0);
        else if (sta == 1)
                gpio_set_value(leddev.gpio_led, 1);        
}

static int led_gpio_init(struct device_node *nd)
{
        int ret = 0;

        /* 获取并申请和设置GPIO */
        leddev.gpio_led = of_get_named_gpio(nd, "led-gpio", 0);
        if(!gpio_is_valid(leddev.gpio_led)) {
                printk("led-gpio is not valid\n");
                return -EINVAL;
        }

        ret = gpio_request(leddev.gpio_led, "LED0");
        if (ret) {
                printk("failed to request led-gpio\n");
                return ret;
        }

        /* 设置GPIO为输出模式并设置其初始状态 */
        gpio_direction_output(leddev.gpio_led, 1);

        return ret;
}

static int led_open(struct inode *inode, struct file *filp)
{
        return 0;
}

static int led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int retvalue = 0;
        unsigned char databuf[1];
        unsigned char ledstat;

        retvalue = copy_from_user(databuf, buf, cnt);
        if (retvalue != 0) {
                printk("copy_from_user failed\n");
                return -EFAULT;
        }

        /* 转换并执行按键操作 */
        ledstat = databuf[0];
        if (ledstat == LEDON) {
                led_switch(LEDON);
        } else if(ledstat == LEDOFF) {
                led_switch(LEDOFF);
        }

        return retvalue;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
};

static int led_probe(struct platform_device *platdev)
{
        int ret = 0;

        printk("led dirver and device have been matched");
        
        /* 申请并初始化LED */
        ret = led_gpio_init(platdev->dev.of_node);
        if (ret < 0)
                return ret;
        
        /* 1、设置设备号 */
	ret = alloc_chrdev_region(&leddev.devid, 0, LED_CNT, LED_NAME);
	if(ret < 0) {
		pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", LED_NAME, ret);
		goto free_gpio;
	}
	
	/* 2、初始化cdev  */
	leddev.cdev.owner = THIS_MODULE;
	cdev_init(&leddev.cdev, &led_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&leddev.cdev, leddev.devid, LED_CNT);
	if(ret < 0)
		goto del_unregister;
	
	/* 4、创建类      */
	leddev.class = class_create(THIS_MODULE, LED_NAME);
	if (IS_ERR(leddev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	leddev.device = device_create(leddev.class, NULL, leddev.devid, NULL, LED_NAME);
	if (IS_ERR(leddev.device)) {
		goto destroy_class;
	}
	
	return 0;
destroy_class:
	class_destroy(leddev.class);
del_cdev:
	cdev_del(&leddev.cdev);
del_unregister:
	unregister_chrdev_region(leddev.devid, LED_CNT);
free_gpio:
	gpio_free(leddev.gpio_led);

	return -EIO;
}

/* platform驱动remove函数，移除驱动时执行 */
static int led_remove(struct platform_device *dev)
{
        gpio_set_value(leddev.gpio_led, 1);
        gpio_free(leddev.gpio_led);
        cdev_del(&leddev.cdev);
        unregister_chrdev_region(leddev.devid, LED_CNT);
        device_destroy(leddev.class, leddev.devid);
        class_destroy(leddev.class);

        return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
        { .compatible = "alientdk,led"},
        { /* Sentinel */}
};

MODULE_DEVICE_TABLE(of, led_of_match);

/* platform驱动结构体 */
static struct platform_driver led_driver = {
        .driver = {
                .name = "stm32mp1-led",
                .of_match_table = led_of_match,
        },
        .probe  = led_probe,
        .remove = led_remove,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init leddev_init(void)
{
	return platform_driver_register(&led_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit leddev_exit(void)
{
        platform_driver_unregister(&led_driver);
}

module_init(leddev_init);
module_exit(leddev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("goku-son");
MODULE_INFO(intree, "Y");