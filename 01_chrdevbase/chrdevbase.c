/*
 * @Date: 2024-11-05
 * @LastEditors: GoKo Son626
 * @LastEditTime: 2024-11-07
 * @FilePath: /01_chrdevbase/chrdevbase.c
 * @Description: 
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

static int __init chrdevbase_init(void)
{
        int ret = 0;

        printk("chrdevbase init!\r\n");
        return ret;
}

static void __exit chrdevbase_exit(void)
{
        printk("chrdevbase exit!\r\n");
}

/* 驱动的注册与卸载 */
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);