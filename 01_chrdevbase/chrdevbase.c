/*
 * @Date: 2024-11-05
 * @LastEditors: GoKo-Son626
 * @LastEditTime: 2024-11-29
 * @FilePath: /1-STM32MP157/Drivers_MP157/01_chrdevbase/chrdevbase.c
 * @Description: 
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define MAXSIZE 100

static char readbuf[MAXSIZE]; /* 读缓冲 */
static char writebuf[MAXSIZE]; /* 写缓冲 */
static char kerneldata[] = {"Hello wangshuqi"};

static int chrdevbase_open(struct inode *inode, struct file *filp)
{
        printk("chrdevbase open!\n");

        return 0;
};

static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;
	
	/* 向用户空间发送数据 */
	memcpy(readbuf, kerneldata, sizeof(kerneldata));
	retvalue = copy_to_user(buf, readbuf, sizeof(kerneldata));//
	if(retvalue == 0){
		printk("kernel senddata ok!\r\n");
	}else{
		printk("kernel senddata failed!\r\n");
	}
	
	printk("chrdevbase read!\r\n");

	return 0;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int retvalue = 0;

	/* 接收用户空间传递给内核的数据并且打印出来 */
	retvalue = copy_from_user(writebuf, buf, cnt);
	if(retvalue == 0){
		printk("kernel recevdata:%s\r\n", writebuf);
	}else{
		printk("kernel recevdata failed!\r\n");
	}
	
	printk("chrdevbase write!\r\n");

	return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{
	printk("chrdevbase release！\r\n");

	return 0;
}

/*
 * 设备操作函数结构体
 */
static struct file_operations chrdevbase_fops = {
	.owner = THIS_MODULE,	
	.open = chrdevbase_open,
	.read = chrdevbase_read,
	.write = chrdevbase_write,
	.release = chrdevbase_release,
};

static int __init chrdevbase_init(void)
{
        int ret = 0;

        /* 注册字符设备驱动 */
        ret = register_chrdev(200, "chrdevbase", &chrdevbase_fops);
        if (ret < 0) {
                printk("Failed to register chrdevbase!\n");
                return ret;
        }

        printk("chrdevbase init!\r\n");
        
        return ret;
}

static void __exit chrdevbase_exit(void)
{
        /* 注销字符设备驱动 */
        unregister_chrdev(200, "chrdevbase");

        printk("chrdevbase exit!\r\n");
}

/* 驱动的注册与卸载 */
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);

/* 
 * LICENSE和作者信息
 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("GoKu");
MODULE_INFO(intree, "Y");