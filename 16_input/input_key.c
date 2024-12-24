#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#define KEYINPUT_NAME		"keyinput"	/* 名字 		*/

struct key_dev {
        struct input_dev *idev;
        struct timer_list timer;
        int key_gpio;
        int key_irq;
};

static struct key_dev keydev;

static irqreturn_t key_interrupt(int irq, void *dev_id)
{
        if (keydev.key_irq != irq)
                return IRQ_NONE;

        /* 定时器延时防抖 */
        disable_irq_nosync(irq);       /* 禁止按键中断 */
        mod_timer(&keydev.timer, jiffies + msecs_to_jiffies(15));

        return IRQ_HANDLED;
}

static int key_gpio_init(struct device_node *nd)
{
        int ret;
        unsigned long irq_flags;
	
	/* 从设备树中获取GPIO */
	keydev.key_gpio = of_get_named_gpio(nd, "keydev-gpio", 0);
	if(!gpio_is_valid(keydev.key_gpio)) {
		printk("key：Failed to get keydev-gpio\n");
		return -EINVAL;
	}
	
	/* 申请使用GPIO */
	ret = gpio_request(keydev.key_gpio, "KEY0");
        if (ret) {
                printk(KERN_ERR "keydev: Failed to request keydev-gpio\n");
                return ret;
	}	
	
	/* 将GPIO设置为输入模式 */
        gpio_direction_input(keydev.key_gpio);
	
	/* 获取GPIO对应的中断号 */
	keydev.key_irq = irq_of_parse_and_map(nd, 0);
	if(!keydev.key_irq) {
                return -EINVAL;
        }

        /* 获取设备树中指定的中断触发类型 */
	irq_flags = irq_get_trigger_type(keydev.key_irq);
	if (IRQF_TRIGGER_NONE == irq_flags)
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
		
	/* 申请中断 */
	ret = request_irq(keydev.key_irq, key_interrupt, irq_flags, "Key0_IRQ", NULL);
	if (ret) {
                gpio_free(keydev.key_gpio);
                return ret;
        }

	return 0;
}

static void key_timer_function(struct timer_list *arg)
{
        int val;

        /* 读取按键值并上报按键事件 */
        val = gpio_get_value(keydev.key_gpio);
        input_report_key(keydev.idev, KEY_0, !val);
        input_sync(keydev.idev);

        enable_irq(keydev.key_irq);
}

static int goku_key_probe(struct platform_device *pdev)
{
        int ret;

        /* 初始化GPIO */
        ret = key_gpio_init(pdev->dev.of_node);
        if (ret < 0)
                return ret;
        /* 初始化定时器 */
        timer_setup(&keydev.timer, key_timer_function, 0);

        /* 申请input_dev */
        keydev.idev = input_allocate_device();
        keydev.idev->name = KEYINPUT_NAME;

#if 0

#endif
        
#if 0

#endif

	keydev.idev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(keydev.idev, EV_KEY, KEY_0);

	/* 注册输入设备 */
	ret = input_register_device(keydev.idev);
	if (ret) {
		printk("register input device failed!\r\n");
		goto free_gpio;
	}
	
	return 0;
free_gpio:
	free_irq(keydev.key_irq,NULL);
	gpio_free(keydev.key_gpio);
	del_timer_sync(&keydev.timer);
	return -EIO;
}

static int goku_key_remove(struct platform_device *pdev)
{
        free_irq(keydev.key_irq,NULL);
        gpio_free(keydev.key_gpio);
        del_timer_sync(&keydev.timer);
        input_unregister_device(keydev.idev);

        return 0;
}

static const struct of_device_id key_of_match[] = {
        {.compatible = "alientek,keydev"},
        {/* Sentinel */}
};

static struct platform_driver goku_key_driver = {
        .driver = {
                .name = "stm32mp1-keydev",
                .of_match_table = key_of_match,
        },
        .probe = goku_key_probe,
        .remove = goku_key_remove,
};

module_platform_driver(goku_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GOKU-SON");
MODULE_INFO(intree, "Y");