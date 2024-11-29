/*
 * @Date: 2024-11-21
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-11-29
 * @FilePath: /1-STM32MP157/Drivers_MP157/03_newled/newled.c
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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define NEWCHRLED_CNT                    1
#define NEWCHRLED_NAME                  "newchrled"
#define LEDOFF                          0
#define LEDON                           1

/* 寄存器物理地址 */
#define PERIPH_BASE     		     	(0x40000000)
#define MPU_AHB4_PERIPH_BASE			(PERIPH_BASE + 0x10000000)
#define RCC_BASE        		    	(MPU_AHB4_PERIPH_BASE + 0x0000)	
#define RCC_MP_AHB4ENSETR				(RCC_BASE + 0XA28)
#define GPIOI_BASE						(MPU_AHB4_PERIPH_BASE + 0xA000)	
#define GPIOI_MODER      			    (GPIOI_BASE + 0x0000)	
#define GPIOI_OTYPER      			    (GPIOI_BASE + 0x0004)	
#define GPIOI_OSPEEDR      			    (GPIOI_BASE + 0x0008)	
#define GPIOI_PUPDR      			    (GPIOI_BASE + 0x000C)	
#define GPIOI_BSRR      			    (GPIOI_BASE + 0x0018)

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/* newchrled设备结构体 */
struct newchrled_dev{
	dev_t devid;			/* 设备号 	 */
	struct cdev cdev;		/* cdev 	*/
	struct class *class;		/* 类 		*/
	struct device *device;	/* 设备 	 */
	int major;				/* 主设备号	  */
	int minor;				/* 次设备号   */
};

struct newchrled_dev newchrled;         //led device

void led_switch(u8 sta) {
        	u32 val = 0;
	if(sta == LEDON) {
		val = readl(GPIOI_BSRR_PI);
		val |= (1 << 16);	
		writel(val, GPIOI_BSRR_PI);
	}else if(sta == LEDOFF) {
		val = readl(GPIOI_BSRR_PI);
		val|= (1 << 0);	
		writel(val, GPIOI_BSRR_PI);
	}	
}

void led_unmap(void) {
        /* 取消映射 */
        iounmap(MPU_AHB4_PERIPH_RCC_PI);
        iounmap(GPIOI_MODER_PI);
        iounmap(GPIOI_OTYPER_PI);
        iounmap(GPIOI_OSPEEDR_PI);
        iounmap(GPIOI_PUPDR_PI);
        iounmap(GPIOI_BSRR_PI);
}

static int led_open(struct inode *inode, struct file *filp) {
        filp->private_data = &newchrled; /* 设置私有数据 */
	return 0;
};

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt) {
        return 0;
}

static ssize_t led_write(struct file *filp, const char *buf, size_t cnt, loff_t *offt) {
        int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) {
		led_switch(LEDOFF);	/* 关闭LED灯 */
	}
	return 0;
}

static int led_release(struct inode *inode, struct file *filp) {
        return 0;
}

/* 设备操作函数 */
static struct file_operations newchrled_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

// static int __int led_init(void) {
static int __init led_init(void) {
        u32 val = 0;
        int ret;

        /* 初始化led */
        /* 1.寄存器地址映射 */
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
    GPIOI_MODER_PI = ioremap(GPIOI_MODER, 4);
    GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER, 4);
    GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR, 4);
    GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR, 4);
    GPIOI_BSRR_PI = ioremap(GPIOI_BSRR, 4);

    /* 2、使能PI时钟 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的设置 */
    val |= (0X1 << 8);  /* 设置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 3、设置PI0通用的输出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0);  /* bit0:1设置01 */
    writel(val, GPIOI_MODER_PI);

    /* 3、设置PI0为推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，设置为上拉*/
    writel(val, GPIOI_OTYPER_PI);

    /* 4、设置PI0为高速。*/
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x2 << 0); /* bit0:1 设置为10*/
    writel(val, GPIOI_OSPEEDR_PI);

    /* 5、设置PI0为上拉。*/
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零*/
    val |= (0x1 << 0); /*bit0:1 设置为01*/
    writel(val,GPIOI_PUPDR_PI);

    /* 6、默认关闭LED */
    val = readl(GPIOI_BSRR_PI);
    val |= (0x1 << 0);
    writel(val, GPIOI_BSRR_PI);

    /* 注册字符设备驱动*/
    	/* 1、创建设备号 */
	if (newchrled.major) {		/*  定义了设备号 */
		newchrled.devid = MKDEV(newchrled.major, 0);
		ret = register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
		if(ret < 0) {
			pr_err("cannot register %s char driver [ret=%d]\n",NEWCHRLED_NAME, NEWCHRLED_CNT);
			goto fail_map;
		}
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);	/* 申请设备号 */
		if(ret < 0) {
			pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", NEWCHRLED_NAME, ret);
			goto fail_map;
		}
		newchrled.major = MAJOR(newchrled.devid);	/* 获取分配号的主设备号 */
		newchrled.minor = MINOR(newchrled.devid);	/* 获取分配号的次设备号 */
	}
	printk("newcheled major=%d,minor=%d\r\n",newchrled.major, newchrled.minor);	
	
	/* 2、初始化cdev */
	newchrled.cdev.owner = THIS_MODULE;
	cdev_init(&newchrled.cdev, &newchrled_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);
	if(ret < 0)
		goto del_unregister;
		
	/* 4、创建类 */
	newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.class)) {
		goto del_cdev;
	}

	/* 5、创建设备 */
	newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.device)) {
		goto destroy_class;
	}
	
	return 0;

destroy_class:
	class_destroy(newchrled.class);
del_cdev:
	cdev_del(&newchrled.cdev);
del_unregister:
	unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
fail_map:
	led_unmap();
	return -EIO;
}

static void __exit led_exit(void){
        led_unmap();

        /* 注销字符设备驱动 */
        cdev_del(&newchrled.cdev);      // 删除cdev
        unregister_chrdev_region(newchrled.devid, 1); // 释放设备号

        device_destroy(newchrled.class, newchrled.devid); // 删除设备
        class_destroy(newchrled.class); // 删除设备类
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
