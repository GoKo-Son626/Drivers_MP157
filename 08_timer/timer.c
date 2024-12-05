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
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define TIMERDEV_CNT               1
#define TIMERDEV_NAME        "mp1-timer"
#define LEDON                   1
#define LEDOFF                  0
#define CLOSE_CMD 		(_IO(0XEF, 0x1))	/* 关闭定时器 */
#define OPEN_CMD		(_IO(0XEF, 0x2))	/* 打开定时器 */
#define SETPERIOD_CMD	        (_IO(0XEF, 0x3))	/* 设置定时器周期命令 */

/* timerdev设备结构体 */
struct timer_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        struct timer_list timer;        /* 定时器 */
        int led_gpio;                   /* led的gpio */
        int timeperiod;                 /* 定时周期 */
        spinlock_t lock;                /* 定义自旋锁 */
};

struct timer_dev timerdev;	        /* timer设备 */

/* led灯初始化 */
static int led_init(void) {
        int ret;
        const char *str;

        /* 设置LED所使用的GPIO */
        // 1. 获取设备节点
        timerdev.nd = of_find_node_by_path("/mp1-gpioled");
        if (timerdev.nd == NULL) {
                pr_err("gpioled node not find!\r\n");
                return -EINVAL;
        }
        
        /* 2. 读取status属性 */
        ret = of_property_read_string(timerdev.nd, "status", &str);
        if (ret < 0) {
                ret = -EINVAL;
        }

        if (strcmp(str, "okay") != 0){
                printk("gpioled: status is okay\n");
                return -EINVAL;
        }
        
        /* 3、获取compatible属性值并进行匹配 */
	ret = of_property_read_string(timerdev.nd, "compatible", &str);
	if(ret < 0) {
		printk("timerdev: Failed to get compatible property\n");
		return -EINVAL;
	}

        if (strcmp(str, "goku-son, led")) {
                printk("timerdev: Compatible match failed\n");
                return -EINVAL;
        }

        /* 4、 获取设备树中的gpio属性，得到led-gpio所使用的led编号 */
	timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
	if(timerdev.led_gpio < 0) {
		printk("can't get led-gpio");
		return -EINVAL;
	}
	printk("led-gpio num = %d\r\n", timerdev.led_gpio);

	/* 5.向gpio子系统申请使用GPIO */
	ret = gpio_request(timerdev.led_gpio, "led");
        if (ret) {
                printk(KERN_ERR "timerdev: Failed to request led-gpio\n");
                return ret;
	}

		/* 6、设置PI0为输出，并且输出高电平，默认关闭LED灯 */
	ret = gpio_direction_output(timerdev.led_gpio, 1);
	if(ret < 0) {
		printk("can't set gpio!\r\n");
		return ret;
	}
        
        printk("Finished led_init\r\n");

	return 0;
}

static int timer_open(struct inode *inode, struct file *filp)
{
        int ret = 0;

	filp->private_data = &timerdev; /* 设置私有数据 */

        timerdev.timeperiod = 1000; /* 默认周期为1s */
        
        ret = led_init();
        if (ret < 0) {
                printk("led_init failed, ret = %d\n", ret);
                return ret;
        }

	return 0;
}

static long timer_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        struct timer_dev *dev = (struct timer_dev *)file->private_data;
        int timerperiod;
        unsigned long flags;

        switch (cmd) {
                case CLOSE_CMD:
                        /* 关闭定时器 */
                        del_timer_sync(&dev->timer);
                        break;
                case OPEN_CMD:
                        /* 打开定时器 */
                        spin_lock_irqsave(&dev->lock, flags);
                        timerperiod = dev->timeperiod;
                        spin_unlock_irqrestore(&dev->lock, flags);
                        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
			break;
                case SETPERIOD_CMD:
                        /* 设置定时器周期 */
                        spin_lock_irqsave(&dev->lock, flags);
                        timerperiod = arg;
                        spin_unlock_irqrestore(&dev->lock, flags);
                        dev->timeperiod = timerperiod;
                        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
                        break;
                default:
                        return -ENOTTY;
        }

        return 0;
}

static int timer_release(struct inode *inode, struct file *filp)
{
        struct timer_dev *dev = filp->private_data;

        /* 关闭LED */
        gpio_set_value(dev->led_gpio, 1);
        /* 释放gpio */
        gpio_free(dev->led_gpio);
        /* 关闭定时器 */
        del_timer_sync(&dev->timer);

	return 0;
}

/* 设备操作函数 */
static struct file_operations timerdev_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
        .unlocked_ioctl = timer_unlocked_ioctl,
	.release = timer_release,
};

/* 定时器处理函数 */
static void timer_func(struct timer_list *arg) {
        struct timer_dev *dev = from_timer(dev, arg, timer);
        int timerperiod;
        unsigned long flags;

        
        static int status = 1;

        /* 反转LED灯 */
        status = !status;
        gpio_set_value(dev->led_gpio, status);

        /* 重启定时器 */
        spin_lock_irqsave(&dev->lock, flags);
	timerperiod = dev->timeperiod;
	spin_unlock_irqrestore(&dev->lock, flags);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod)); 
}


/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init timer_init(void)
{
	int ret;

        /* 初始化自旋锁 */
        spin_lock_init(&timerdev.lock);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (timerdev.major) {		/*  定义了设备号 */
		timerdev.devid = MKDEV(timerdev.major, 0);
		ret = register_chrdev_region(timerdev.devid, TIMERDEV_CNT, TIMERDEV_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", TIMERDEV_NAME, TIMERDEV_CNT);
			return ret;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&timerdev.devid, 0, TIMERDEV_CNT, TIMERDEV_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", TIMERDEV_NAME, ret);
			return ret;
		}
		timerdev.major = MAJOR(timerdev.devid);	/* 获取分配号的主设备号 */
		timerdev.minor = MINOR(timerdev.devid);	/* 获取分配号的次设备号 */
	}
	printk("timerdev major=%d,minor=%d\r\n",timerdev.major, timerdev.minor);	
	
	/* 2、初始化cdev */
	timerdev.cdev.owner = THIS_MODULE;
	cdev_init(&timerdev.cdev, &timerdev_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&timerdev.cdev, timerdev.devid, TIMERDEV_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	timerdev.class = class_create(THIS_MODULE, TIMERDEV_NAME);
	if (IS_ERR(timerdev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMERDEV_NAME);
	if (IS_ERR(timerdev.device)) {
		goto destroy_class;
	}

        /* 6. 初始化定时器 */
        timer_setup(&timerdev.timer, timer_func, 0);

        return 0;
	
destroy_class:
	class_destroy(timerdev.class);
del_cdev:
	cdev_del(&timerdev.cdev);
del_unregister:
	unregister_chrdev_region(timerdev.devid, TIMERDEV_CNT);

	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit timer_exit(void)
{
        /* 删除定时器 */
        del_timer_sync(&timerdev.timer);
#if 0
        del_timer(&timerdev.timer);
#endif

	/* 注销字符设备驱动 */
	cdev_del(&timerdev.cdev);/*  删除cdev */
	unregister_chrdev_region(timerdev.devid, TIMERDEV_CNT); /* 注销设备号 */

	device_destroy(timerdev.class, timerdev.devid);/* 注销设备 */
	class_destroy(timerdev.class);/* 注销类 */
}

module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("goku-son");
MODULE_INFO(intree, "Y");
