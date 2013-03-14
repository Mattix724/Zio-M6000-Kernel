/***************************************************
 * trackball.c
 * Copyright (C) 2010 Compalcomm, Inc.
 * Author: Johnny Lee
 * Created on: 1 Feb, 2010
 * Description: CCI KB60 TrackBall(Jogball) Driver.
 ***************************************************/

/*******************************************************************************
 * Includes
 *******************************************************************************/
#include <linux/module.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/hrtimer.h>

#ifdef CONFIG_PM
#include <mach/vreg.h>
#endif

#include "trackball_i.h"

/*******************************************************************************
 * DEFINITIONS
 *******************************************************************************/
/*GPIO pin definitions*/
#define TRACKBALL_KEY_GPIO_UP           29
#define TRACKBALL_KEY_GPIO_DOWN         28
#define TRACKBALL_KEY_GPIO_LEFT         27
#define TRACKBALL_KEY_GPIO_RIGHT        35
#define TRACKBALL_KEY_GPIO_RIGHT_SPRINT 20

/*Debug Message Flag*/
#define CCI_TRACKBALL_DEBUG 0


/*******************************************************************************
 * MACROS
 *******************************************************************************/
#define IRQ_TO_MSM(n) (n-64)
#define MSM_TO_IRQ(n) (n+64)

#if (CCI_TRACKBALL_DEBUG == 1)
    #define jb_info(fmt, arg...) \
        printk(KERN_INFO "[Trackball] " fmt, ##arg)
#else
    #define jb_info(fmt, arg...) \
        ({ if (0) printk(KERN_INFO fmt, ##arg); 0; })
#endif

/*******************************************************************************
 * STRUCTURES AND ENUMS
 *******************************************************************************/
struct trackball_keytype
{
    int gpio_num;
    char *name;
};

enum jb_define
{
    JB_UP    = 0,
    JB_DOWN  = 1,
    JB_LEFT  = 2,
    JB_RIGHT = 3,
    JB_MAX   = 4
};


/*******************************************************************************
 * Local Variable Declaration
 *******************************************************************************/
static struct input_dev *jdev;
static spinlock_t jblock;
unsigned long ts_active_time = INITIAL_JIFFIES;
unsigned long key_active_time = INITIAL_JIFFIES;
void cci_key_input_disable_delay(unsigned);
void cci_ts_input_disable_delay(unsigned);

/*******************************************************************************
 * Structures
 *******************************************************************************/
static struct trackball_keytype jog_def[JB_MAX] =
{
	/*GPIO                    NAME */
	{TRACKBALL_KEY_GPIO_UP,    "UP"   },
	{TRACKBALL_KEY_GPIO_DOWN,  "DOWN" },
	{TRACKBALL_KEY_GPIO_LEFT,  "LEFT" },
	{TRACKBALL_KEY_GPIO_RIGHT, "RIGHT"},
};

/*******************************************************************************
 * Functions
 *******************************************************************************/
 void cci_ts_input_disable_delay(unsigned delay)
{
    unsigned long new_jiffies = (unsigned long)(jiffies + delay);
    if (delay > 2000) return;
    if (time_after(new_jiffies, ts_active_time)) ts_active_time = new_jiffies;
}
 
void cci_key_input_disable_delay(unsigned delay)
{
    unsigned long new_jiffies = (unsigned long)(jiffies + delay);
    if (delay > 2000) return;
    if (time_after(new_jiffies, key_active_time)) key_active_time = new_jiffies;
}
static int trackball_gpio_init(bool on)
{
	int rc = 0;
	int i;

	if (on) {
		for(i = 0; i < JB_MAX; i++) {
			rc = gpio_request(jog_def[i].gpio_num, "cci_trackball");
			if(rc < 0) {
				printk(KERN_ERR "%s: fail to request gpio for trackball (%s)! error = %d\n", __func__, jog_def[i].name, rc);
			}

			rc = gpio_direction_input(jog_def[i].gpio_num);
			if(rc < 0) {
				printk(KERN_ERR "%s: fail to set direction of trackball gpio (%s)! error = %d\n", __func__, jog_def[i].name, rc);
			}
		}
	} else {
		for (i = 0; i < JB_MAX; i++) {
			gpio_free(jog_def[i].gpio_num);
		}
	}
	return rc;
}

static int trackball_irq_enable(bool on)
{
	unsigned long irqflags = 0;
	int i;
	spin_lock_irqsave(&jblock,irqflags);
	if (on) {
		for(i = 0; i < JB_MAX; i++) {
			enable_irq(MSM_GPIO_TO_INT(jog_def[i].gpio_num));
		}
	} else {
		for (i = 0; i < JB_MAX; i++) {
			disable_irq_nosync(MSM_GPIO_TO_INT(jog_def[i].gpio_num));
		}
	}
	spin_unlock_irqrestore(&jblock,irqflags);
	return 0;
}

static void trackball_power(bool on)
{
	int rc;
	struct vreg *vreg_trackball;

	vreg_trackball = vreg_get(NULL, "synt");

	if (on) {
		rc = vreg_set_level(vreg_trackball,2600);
		if (rc) {
			printk(KERN_ERR "%s: vreg_set_level failed (%d)\n", __func__, rc);
		}
		rc = vreg_enable(vreg_trackball);
		if (rc) {
			printk(KERN_ERR "%s: vreg_enable failed (%d)\n", __func__, rc);
		}
	} else {
		rc = vreg_disable(vreg_trackball);
		if (rc) {
			printk(KERN_ERR "%s: vreg_disable failed (%d)\n", __func__, rc);
		}
	}
}

#ifdef CONFIG_PM
static int trackball_suspend(struct platform_device *dev, pm_message_t state)
{
	jb_info("%s\n", __func__);

	trackball_irq_enable(false);
	trackball_power(false);
	trackball_gpio_init(false);

	return 0;
}

static int trackball_resume(struct platform_device *dev)
{
	jb_info("%s\n", __func__);

	trackball_gpio_init(true);
	trackball_power(true);
	trackball_irq_enable(true);

	return 0;
}
#endif

static void trackball_report_event(int g, int p)
{
	if (p == JB_UP) { //up
		input_report_rel(jdev,REL_Y,-g);
	} else if (p == JB_DOWN) { //dn
		input_report_rel(jdev,REL_Y,g);
	} else if (p == JB_LEFT) { //L
		input_report_rel(jdev,REL_X,-g);
	} else if (p == JB_RIGHT) { //R
		input_report_rel(jdev,REL_X,g);
	}
	input_sync(jdev);
	jb_info("%s:  p:%d, g:%d\n",__func__,p,g);
}

static irqreturn_t trackball_irqhandler_direction_key(int irq, void *dev_id)
{
	int i;
	int jog_index;

	/* disable four IRQs */
	trackball_irq_enable(false);
	/* check the direction of this IRQ */
	for(i = 0, jog_index = -1; i < JB_MAX; i++) {
		if (jog_def[i].gpio_num == IRQ_TO_MSM(irq)) {
		jog_index = i;
		break;
		}
	}
	if (jog_index != -1) {
		struct trackball_filter_packet filter_packet;
		filter_packet.time = jiffies;
		filter_packet.way = jog_index;
		trackball_filter_send_packet(filter_packet);

		jb_info("-----trackball detection %s, j:%ld\n", jog_def[jog_index].name, jiffies);
	}
   //B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
	cci_key_input_disable_delay(30);
	cci_ts_input_disable_delay(30);
   //E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.

	/* enable four IRQs */
	trackball_irq_enable(true);

	return IRQ_HANDLED;
}

static struct trackball_filter_info trackball_filter = {
	.time = 1000,
	.step = 65,
	.deb = 3,
	.limit = 4,
	.report = trackball_report_event,
};

static struct platform_device trackball_device = {
	.name = "trackball_driver",
};

static struct platform_driver trackball_driver = {
#ifdef CONFIG_PM
	.suspend	= trackball_suspend,
	.resume		= trackball_resume,
#endif
	.driver		= {
		.name	= "trackball_driver",
	},
};

static int __init trackball_input_sys_init(void)
{
	int res;
	jdev = input_allocate_device();
	jdev->name = "Trackball";

	set_bit(EV_KEY, jdev->evbit);
	set_bit(EV_REL, jdev->evbit);

	set_bit(BTN_MOUSE, jdev->keybit);
	set_bit(REL_X, jdev->relbit);
	set_bit(REL_Y, jdev->relbit);
	res = input_register_device(jdev);
	if (res < 0) {
		printk(KERN_ERR "%s: register to input subsystem fail! err = %d. \n", __func__, res);
	}

	return res;
}

static int __init trackball_irq_init(void)
{
	unsigned long irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	int rc = 0;
	int i;
	for(i = 0; i < JB_MAX; i++) {
		rc = request_irq(MSM_GPIO_TO_INT(jog_def[i].gpio_num), trackball_irqhandler_direction_key, irqflags, "cci trackball", NULL);
		if(rc < 0) {
			printk(KERN_ERR "%s: fail to init irq for trackball (%s)! error = %d\n", __func__, jog_def[i].name, rc);
		}
	}
	return rc;
}

static void __init cci_trackball_hw_detect(void)
{
#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
	extern int HW_ID4;

	if (HW_ID4 == 0) {
		jog_def[JB_RIGHT].gpio_num = TRACKBALL_KEY_GPIO_RIGHT;
		printk(KERN_INFO "%s:  default KEY_RIGHT gpio\n", __func__);
	} else {
		jog_def[JB_RIGHT].gpio_num = TRACKBALL_KEY_GPIO_RIGHT_SPRINT;
		printk(KERN_INFO "%s:  Sprint KEY_RIGHT gpio\n", __func__);;
	}
#else
	jog_def[JB_RIGHT].gpio_num = JOGBALL_KEY_GPIO_RIGHT;
	printk(KERN_INFO "%s:  default KEY_RIGHT gpio (no HW_ID4)\n", __func__);
#endif
	return;
}

static int __init cci_trackball_init(void)
{
	jb_info("%s\n", __func__);
	spin_lock_init(&jblock);
	cci_trackball_hw_detect();
	trackball_filter_init(trackball_filter);
	trackball_power(true);
	trackball_input_sys_init();
	trackball_gpio_init(true);
	trackball_irq_init();

	platform_device_register(&trackball_device);
	return platform_driver_register(&trackball_driver);
}

module_init(cci_trackball_init);

MODULE_DESCRIPTION("CCI Trackball Driver");
MODULE_AUTHOR("Johnny Lee");
MODULE_LICENSE("GPL v2");
