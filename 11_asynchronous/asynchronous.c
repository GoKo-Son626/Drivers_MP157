/*
 * @Date: 2024-12-11
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-18
 * @FilePath: /1-STM32MP157/Drivers_MP157/11_asynchronous/asynchronous.c
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
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fcntl.h>

#define KEY0_CNT       1
#define KEY0_NAME        "mp1-keydev"

/* 定义按键值 */
enum key_status {
        KEY_PRESS = 0,      /* 按键按下 */
        KEY_RELEASE,        /* 按键松开 */
        KEY_KEEP,           /* 按键状态保持 */
        KEY_ERROR,          /* 按键出错 */
};

/* gpioled设备结构体 */
struct key_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        
        int key0_gpio;                  /* 使用的GPIO编号 */
        struct timer_list timer;        /* 按键值 */
        int irq_num;                  /* 中断号 */
        atomic_t status;                /* 按键状态 */
        wait_queue_head_t r_wait;        /* 读等待队列头 */
        struct fasync_struct *async_queue; /*   fasyc_struct结构体 */
};

struct key_dev keydev;	        /* led设备 */

/* 中断调用定时器开启延时15ms做按键防抖处理 */
static irqreturn_t key_interrupt(int irq, void *dev_id)
{
        mod_timer(&keydev.timer, jiffies + msecs_to_jiffies(15));
        return IRQ_HANDLED;
}

/*  */
static void key0_timer_function(struct timer_list *arg)
{
        static int last_val = 1;
        int key_val;

        /* 读取按键值并判断当前状态 */
        key_val = gpio_get_value(keydev.key0_gpio);
        if (key_val == 0 && last_val) {
                atomic_set(&keydev.status, KEY_PRESS);
                wake_up_interruptible(&keydev.r_wait);
                if (keydev.async_queue) {
                        kill_fasync(&keydev.async_queue, SIGIO, POLL_IN);
                }
                
        } else if (key_val == 1 && !last_val) {
                atomic_set(&keydev.status, KEY_RELEASE);
                wake_up_interruptible(&keydev.r_wait);
                if (keydev.async_queue) {
                        kill_fasync(&keydev.async_queue, SIGIO, POLL_IN);
                }
                
        } else
                atomic_set(&keydev.status, KEY_KEEP);
        last_val = key_val;

}

/* 获取设备信息并初始化使用的GPIO引脚 */
static int key_parse_dt(void) {
        int ret;
        const char *str;
        unsigned long irq_flags;
        
        /* 获取设备节点 */
        if ((keydev.nd = of_find_node_by_path("/mp1-key0")) == NULL)
        {
                printk("can't find mp1-keydev node\n");
                return -ENOENT;
        }
        /* 2.读取status属性 */
        if((ret = of_property_read_string(keydev.nd, "status", &str)) < 0) {
                printk("status error\n");
                return -ENOENT;
        }
	/* 3、获取compatible属性值并进行匹配 */
	if((ret = of_property_read_string(keydev.nd, "compatible", &str)) < 0) {
		printk("keydev: Failed to get compatible property\n");
		return -EINVAL;
	}
        if (strcmp(str, "alientek,keydev")) {
                printk("keydev: Not match compatible device\n");
                return -ENOENT;
        }
        /* 4.获取GPIO对应的中断号 */
        if ((keydev.key0_gpio = of_get_named_gpio(keydev.nd, "key0-gpio", 0)) < 0)
        {
                printk("can't find key0-gpio\n");
                return -ENOENT;
        }
        /* 5.获取对应的GPIO中断号并输出 */
        if(!(keydev.irq_num = irq_of_parse_and_map(keydev.nd, 0))){
                return -EINVAL;
        }
	printk("keydev-gpio num = %d\r\n", keydev.key0_gpio);
        /* 6.申请GPIO */
        if ((ret = gpio_request(keydev.key0_gpio, "KEY0"))) {
                printk(KERN_ERR "keydev: Failed to request key0-gpio\n");
                return ret;
        }
        /* 7. 设置GPIO输入模式 */
        gpio_direction_input(keydev.key0_gpio);
        /* 8.获取设备树中指定的中断触发类型 */
        if (IRQF_TRIGGER_NONE == (irq_flags = irq_get_trigger_type(keydev.irq_num)))
                irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
        /* 9.申请中断 */
        if((ret = request_irq(keydev.irq_num, key_interrupt, irq_flags, "Key0_IRQ", NULL))) {
                pr_err("Failed to request IRQ %d, error code: %d\n", keydev.irq_num, ret);
                gpio_free(keydev.key0_gpio);
                return ret;
        }

	return 0;
}

/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做private_data的成员变量
 * 			一般在open的时候将private_data指向设备结构体。
 * @return 		: 0 成功;其他 失败
 */
static int key_open(struct inode *inode, struct file *filp)
{
	//filp->private_data = &keydev; /* 设置私有数据 */
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
static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        int ret;

        /* 加入等待队列，有按键按下或松开时会唤醒 */
        ret = wait_event_interruptible(keydev.r_wait, KEY_KEEP != atomic_read(&keydev.status));
        if(ret)        
                return ret;

        /* 发送按键状态给应用程序并重置状态 */
        ret = copy_to_user(buf, &keydev.status, sizeof(int));
        /* 状态重置 */
        atomic_set(&keydev.status, KEY_KEEP);

	return ret;
}

static int key_fasync(int fd, struct file *filp, int on)
{
        return fasync_helper(fd, filp, on, &keydev.async_queue);
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 		: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t key_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 		: 0 成功;其他 失败
 */
static int key_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static unsigned int key_poll(struct file *filp, struct poll_table_struct *wait)
{
        unsigned int mask = 0;

        poll_wait(filp, &keydev.r_wait, wait);

        if (atomic_read(&keydev.status) == KEY_KEEP) {
                mask = POLLIN | POLLRDNORM;
        }
        
        return mask;
}
/* 设备操作函数 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
        .poll = key_poll,
        .fasync = key_fasync,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init keydev_init(void)
{
	int ret = 0;

        /* 初始化等待队列头 */
        init_waitqueue_head(&keydev.r_wait);

        /* 初始化按键状态 */
        atomic_set(&keydev.status, KEY_KEEP);

        /* 解析设备树信息并初始化GPIO申请中断 */
        if ((ret = key_parse_dt())) {
                return ret;
        }
        
	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (keydev.major) {		/*  定义了设备号 */
		keydev.devid = MKDEV(keydev.major, 0);
		ret = register_chrdev_region(keydev.devid, KEY0_CNT, KEY0_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n", KEY0_NAME, KEY0_CNT);
			goto free_gpio;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&keydev.devid, 0, KEY0_CNT, KEY0_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", KEY0_NAME, ret);
			goto free_gpio;
		}
		keydev.major = MAJOR(keydev.devid);	/* 获取分配号的主设备号 */
		keydev.minor = MINOR(keydev.devid);	/* 获取分配号的次设备号 */
	}
	printk("keydev major=%d,minor=%d\r\n",keydev.major, keydev.minor);	
	
	/* 2、初始化cdev */
	keydev.cdev.owner = THIS_MODULE;
	cdev_init(&keydev.cdev, &key_fops);
	
	/* 3、添加一个cdev */
	cdev_add(&keydev.cdev, keydev.devid, KEY0_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	keydev.class = class_create(THIS_MODULE, KEY0_NAME);
	if (IS_ERR(keydev.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY0_NAME);
	if (IS_ERR(keydev.device)) {
		goto destroy_class;
	}

        /* 6.初始化timer,设置定时器处理函数,还未设置周期，所有不会激活定时器 */
        timer_setup(&keydev.timer, key0_timer_function, 0);

	return 0;
	
destroy_class:
	class_destroy(keydev.class);
del_cdev:
	cdev_del(&keydev.cdev);
del_unregister:
	unregister_chrdev_region(keydev.devid, KEY0_CNT);
free_gpio:
        free_irq(keydev.irq_num, NULL);
        gpio_free(keydev.key0_gpio);

	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit keydev_exit(void)
{
	/* 注销字符设备驱动 */
        free_irq(keydev.irq_num, NULL);
        gpio_free(keydev.key0_gpio);
        del_timer_sync(&keydev.timer);
	cdev_del(&keydev.cdev);/*  删除cdev */
	unregister_chrdev_region(keydev.devid, KEY0_CNT); /* 注销设备号 */
	device_destroy(keydev.class, keydev.devid);/* 注销设备 */
	class_destroy(keydev.class);/* 注销类 */
}

module_init(keydev_init);
module_exit(keydev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("goku-son");
MODULE_INFO(intree, "Y");