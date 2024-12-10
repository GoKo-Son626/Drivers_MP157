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

#define KEY0_CNT       1
#define KEY0_NAME        "mp1-key"

/* gpioled设备结构体 */
struct key_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;	        /* 类 		*/
	struct device *device;	        /* 设备 	 */
	int major;			/* 主设备号	  */
	int minor;			/* 次设备号   */
	struct device_node *nd;         /* 设备节点 */
        int key_gpio;                  /* 使用的GPIO编号 */
        struct timer_list timer;        /* 按键值 */
        int irq_num;                  /* 中断号 */
        spinlock_t spinlock;            /* 自旋锁 */
};

struct key_dev keydev;	        /* led设备 */
static int status = KEY_KEEP;	/* 按键状态 */

/* 获取设备信息并初始化使用的GPIO引脚 */
static int key_parse_dt(void) {
        int ret;
        const char *str;
        
        /* 获取设备节点 */
        if ((key.nd = of_find_node_by_path("/mp1-key0")) == NULL)
        {
                printk("can't find mp1-key node\n");
                return -ENOENT;
        }
        /* 2.读取status属性 */
        if((ret = of_property_read_string(keydev.nd, "status", &str)) < 0) {
                printk("status error\n");
                return -ENOENT;
        }
	/* 3、获取compatible属性值并进行匹配 */
	if((ret = of_property_read_string(key.nd, "compatible", &str)) < 0) {
		printk("key: Failed to get compatible property\n");
		return -EINVAL;
	}
        if (strcmp(str, "alientek,key")) {
                printk("key: Not match compatible device\n");
                return -ENOENT;
        }
        /* 4.获取GPIO对应的中断号 */
        if ((key.key_gpio = of_get_named_gpio(key.nd, "key0-gpio", 0)) < 0)
        {
                printk("can't find key0-gpio\n");
                return -ENOENT;
        }
        /* 5.获取对应的GPIO中断号并输出 */
        if(!(key.irq_num = irq_of_parse_and_map(key.nd, 0))){
                return -EINVAL;
        }
	printk("key-gpio num = %d\r\n", key.key_gpio);
        /* 6.申请GPIO */
        if (ret = gpio_request(keydev.key0_gpio, "KEY0")) {
                printk(KERN_ERR "key: Failed to request key0-gpio\n");
                return ret;
        }
        /* 7. 设置GPIO输入模式 */
        gpio_direction_input(keydev.key0_gpio);
        /* 8.获取设备树中指定的中断触发类型 */
        if (IRQF_TRIGGER_NONE == (irq_flags = irq_get_trigger_type(key.irq_num)))
                irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
        /* 9.申请中断 */
        if(ret = request_irq(key.irq_num, key_interrupt, irq_flags, "Key0_IRQ", NULL)) {
                pr_err("Failed to request IRQ %d, error code: %d\n", key.irq_num, ret);
                gpio_free(key.key_gpio);
                return ret;
        }

	return 0;
}

static void key0_timer_function(struct timer_list *arg)
{
        unsigned long flags;
        int key_val;

        /* 自旋锁上锁 */
        spin_lock_irqsave(&keydev.spinlock, flags);

        /* 读取按键值并判断当前状态 */
        key_val = gpio_get_value(keydev.key_gpio);
        if (key_val == 0 && last_val)
                status = KEY_PRESS;
        else if (key_val == 1 &&!last_val)
                status = KEY_RELEASE;
        else
                status = KEY_KEEP;
        lase_val = key_val;
        
        /* 自旋锁解锁 */
        spin_unlock_irqrestore(&key.spinlock, flags);
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
        spin_lock_irqsave(&key.spinlock, flags);

        /* 发送按键状态给应用程序并重置状态 */
        ret = copy_to_user(buf, &status, sizeof(int));
        status = KEY_KEEP;

        spin_unlock_irqrestorel(&key.spinlock, flags);

	return ret;
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

/* 设备操作函数 */
static struct file_operations key_fops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = key_release,
};

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init key_init(void)
{
	int ret = 0;

        /* 初始化自旋锁 */
        spin_lock_init(&keydev.lock);
        /* 解析设备树信息并初始化GPIO申请中断 */
        if (ret = key_parse_dt()) {
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
        timer_setup(&key.timer, key0_timer_function, 0);

	return 0;
	
destroy_class:
	class_destroy(keydev.class);
del_cdev:
	cdev_del(&keydev.cdev);
del_unregister:
	unregister_chrdev_region(keydev.devid, KEY0_CNT);
free_gpio:
        free_irq(keydev.irq_num, NULL);
        gpio_free(keydev.gpio);

	return -EIO;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit key_exit(void)
{
	/* 注销字符设备驱动 */
        free_irq(keydev.irq_num, NULL);
        gpio_free(keydev.gpio);
	cdev_del(&keydev.cdev);/*  删除cdev */
	unregister_chrdev_region(keydev.devid, KEY0_CNT); /* 注销设备号 */
	device_destroy(keydev.class, keydev.devid);/* 注销设备 */
	class_destroy(keydev.class);/* 注销类 */
}

module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("goku-son");
MODULE_INFO(intree, "Y");