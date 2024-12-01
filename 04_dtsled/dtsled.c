/*
 * @Date: 2024-11-21
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-12-01
 * @FilePath: /1-STM32MP157/Drivers_MP157/04_dtsled/dtsled.c
 * @Description: 
*/

#include <linux/limits.h>
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

#define DTSLED_CNT      1
#define DTSLED_NAME     "DTSLEDNAME"
#define LEDON           1
#define LEDOFF           0

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/* dtsled设备结构体 */
struct dtsled_dev {
        dev_t devid;            /* 设备号 */
        struct cdev cdev;       /* 设备 */
        struct class *class;    /* 设备类 */
        struct device *device;  /* 设备 */        
        int major;              /* 主设备号 */
        int minor;              /* 次设备号 */
};

struct dtsled_dev dtsled;       /* led设备 */
void led_nnmap(void){
        iounmap(MPU_AHB4_PERIPH_RCC_PI);
        iounmap(GPIOI_MODER_PI);
        iounmap(GPIOI_OTYPER_PI);
	iounmap(GPIOI_OSPEEDR_PI);
	iounmap(GPIOI_PUPDR_PI);
	iounmap(GPIOI_BSRR_PI);
}

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

static int dtsled_open(struct inode *inode, struct file *filp) {
        filp->private_data = &dtsled; /* 设置私有数据 */
	return 0;
};

static ssize_t dtsled_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt) {
        return 0;
}

static ssize_t dtsled_write(struct file *filp, const char *buf, size_t cnt, loff_t *offt) {
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

static int dtsled_release(struct inode *inode, struct file *filp) {
        return 0;
}

/* dtsled字符设备操作集 */
static const struct file_operations dtsled_fops = {
        .owner = THIS_MODULE,
        .open = dtsled_open,
	.read = dtsled_read,
	.write = dtsled_write,
	.release = dtsled_release,
};

static int __init dtsled_init(void){
        int ret = 0;

        /* 1. 注册字符设备 */
        dtsled.major = 0;
        if (dtsled.major) {
                dtsled.devid = MKDEV(dtsled.major, 0);
                ret = register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
        } else {
                ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);
                dtsled.major = MAJOR(dtsled.devid);
                dtsled.minor = MINOR(dtsled.devid);
        }
        if(ret < 0) {
                goto fail_devid;
        }

        /* 2. 添加字符设备 */
        dtsled.cdev.owner = THIS_MODULE;        
        cdev_init(&dtsled.cdev, &dtsled_fops);
        ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);
        if (ret < 0) {
                goto fail_cdev;
        }
        
        /* 3. 自动创建设备节点 */
        dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
        if (IS_ERR(dtsled.class))
        {
                ret = PTR_ERR(dtsled.class);
                goto fail_class;
        }
        
        dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
        if(IS_ERR(dtsled.device)) {
                ret = PTR_ERR(dtsled.device);
                goto fail_device;
        }
        return 0;

fail_device:
        class_destroy(dtsled.class);
fail_class:
        cdev_del(&dtsled.cdev);
fail_cdev:
        unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
fail_devid:
        return ret;
}

static void __exit dtsled_exit(void){
        /* 删除字符设备 */
        cdev_del(&dtsled.cdev);
        /* 释放设备号 */
        unregister_chrdev_region(dtsled.devid, DTSLED_CNT);

        /* 摧毁设备 */
        device_destroy(dtsled.class, dtsled.devid);
        /* 摧毁类 */
        class_destroy(dtsled.class);
}

/* 注册和卸载驱动 */
module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("goku-son");