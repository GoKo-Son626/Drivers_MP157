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
#include <linux/timer.h>

#define TIMERDEV_CNT               1
#define TIMERDEV_NAME        "mp1-timer"
#define LEDON                   1
#define LEDOFF                  0

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
};

struct timer_dev timerdev;	        /* timer设备 */

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 			一般在open的时候将private_data指向设备结构体。
 * @return 		: 0 成功;其他 失败
 */
static int timer_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &timerdev; /* 设置私有数据 */
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
static ssize_t timer_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
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
static ssize_t timer_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;

	return retvalue;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 		: 0 成功;其他 失败
 */
static int timer_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数 */
static struct file_operations timerdev_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.read = timer_read,
	.write = timer_write,
	.release = timer_release,
};

/* 定时器处理函数 */
static void timer_func(unsigned long arg) {
        struct timer_dev *dev = (struct timer_dev *)arg;
        static int status = 1;

        status = !status;
        gpio_set_value(dev->led_gpio, status);

}

/* led灯初始化 */
int led_init(struct timer_dev *dev) {
        int ret = 0;

        dev->nd = of_find_node_by_path("/gpioled");
        if (dev->nd == NULL) {
                pr_err("gpioled node not find!\r\n");
                goto fail_fd;
        }
        
        dev->led_gpio = of_get_named_gpio(dev->nd, "ledgpio", 0);
        if (dev->led_gpio < 0) {
                ret = -EINVAL;
                goto fail_gpio;
        }
        
        ret = gpio_request(dev->led_gpio, "led");
        if (ret)
        {
                ret = -EBUSY;
                printk("IO request failed\r\n");
                goto fail_request;
        }

        ret = gpio_direction_output(dev->led_gpio, 1);
        if(ret < 0) {
                ret = -EBUSY;
                goto fail_gpiooutputset;
        }
        
        return ret;

fail_gpiooutputset:
        gpio_free(dev->led_gpio);
fail_request:
fail_gpio:
fail_fd:
        return ret;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init timer_init(void)
{
	int ret = 0;

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
	return 0;

        /* 6. LED灯初始化 */
        led_init(&timerdev);
        if (ret == 0)
        {
                printk("led init\r\n");
                goto fail_ledinit;
        }

        /* 7. 初始化定时器 */
        init_timer(&timerdev.timer);
        timerdev.timer.function = timer_func;
        timerdev.timer.expires = jiffies + msecs_to_jiffies(500);
        timerdev.timer.data = (unsigned long)&timerdev;
        add_timer(&timerdev.timer);

        return 0;
	
fail_ledinit:
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
        del_timer(&timerdev.timer);

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