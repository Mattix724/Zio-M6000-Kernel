/*
 * gsensor.c
 * Copyright (C) 2010 CCI, Inc.
 * Author: Oscar Lee
 * Created on: 06-Feb.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

#include <asm/gpio.h>

#include <linux/cdev.h>
#include "gsensor.h"

#define CCI_GSENSOR_DEBUG 0

#ifdef CCI_GSENSOR_DEBUG
#define pr_gs(fmt, arg...) \
    printk(KERN_INFO fmt, ##arg)
#else
#define pr_gs(fmt, arg...) \
    ({ if (0) printk(KERN_INFO fmt, ##arg); 0; })

#endif

struct work_struct gsensor_work; 
struct wake_lock gs_wake_lock;

static void gsensor_wakeup(struct work_struct *work_reboot){
	pr_gs("============= gsensor wakeup===============\n");
	wake_lock_timeout(&gs_wake_lock, HZ );
}

static irqreturn_t gsensor_irqhandler(int irq, void *dev_id)
{
    schedule_work(&gsensor_work);
	return IRQ_HANDLED;
}

static int init_gsensor_wakeup_irq(void){
	unsigned long irqflags = 0;
	int err=0;
	irqflags = IRQF_TRIGGER_RISING /*|IRQF_TRIGGER_FALLING*/;
	err=request_irq(MSM_GPIO_TO_INT(EXT_GS_WAKUP_GPIO), gsensor_irqhandler, irqflags, "cci kb60 gsensor", NULL);

	if(err < 0)
		printk("error to request a irq for gsensor! error=%d\n", err);

	return err;
}

static int config_gsensor_gpio(void){
	int rc = 0;

	rc = gpio_request(EXT_GS_WAKUP_GPIO,"cci_kb60_gsensor_wakeup");
	if(rc < 0)
		printk("fail to request gpio for gsensor! error = %d\n",rc);

	rc = gpio_direction_input(EXT_GS_WAKUP_GPIO);
	if(rc < 0)
		printk("fail to set direction of gsensor gpio! error = %d\n",rc);

	return 0;
}

static int __init kb60_gsensor_wakeup_init(void)
{
	INIT_WORK(&gsensor_work, gsensor_wakeup); 
	pr_gs("start init kb60_gsensor_wakeup\n");
	// init idle_wake_lock
	wake_lock_init(&gs_wake_lock, WAKE_LOCK_IDLE, "gsensor");
	// configure gsensor gpio
	config_gsensor_gpio();
	// request a irq for gsensor wakeup
	init_gsensor_wakeup_irq(); 
	pr_gs("init gsensor_wakeup successfully\n");
	return 0;
}

static void __exit kb60_gsensor_wakeup_exit(void)
{
	wake_lock_destroy(&gs_wake_lock);
	free_irq(MSM_GPIO_TO_INT(EXT_GS_WAKUP_GPIO), NULL);
	gpio_free(EXT_GS_WAKUP_GPIO);
}

module_init(kb60_gsensor_wakeup_init);
module_exit(kb60_gsensor_wakeup_exit);

MODULE_DESCRIPTION("KB60 gsensor wakeup irq");
MODULE_AUTHOR("Oscar Lee");
MODULE_LICENSE("GPL v2");
