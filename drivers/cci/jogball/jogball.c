/*******************************
 * jogball.c
 * Copyright (C) 2009 CCI, Inc.
 * Author: Johnny Lee
 * Created on: 29-July.
 * Description:
 * For CCI KB60 Track Ball Driver.
 *******************************/


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

#define JOG_PUSH 0

#include "jogball.h"

/*******************************************************************************
 * Local Variable Declaration
 *******************************************************************************/
static struct input_dev *jdev;

#if JOG_PUSH
static int jogball_enter_state = 0;
#endif
static int timer_is_start = 0;
static int jb_direction_count[JOGBALL_DIRECTION_WAY]; /*UP,Down,Left,Right*/
static struct hrtimer timer;
#ifdef CONFIG_PM
static struct hrtimer timer_suspend;
static int jogball_is_suspend = 0; /*This variable is used to avoid interrupts when suspending*/
#endif

/*******************************************************************************
 * Local Functions Declaration
 *******************************************************************************/
static int jogball_irq_init(void);
static void jogball_irq_exit(void);
#ifdef CONFIG_PM
static int jogball_suspend(struct platform_device *dev, pm_message_t state);
static int jogball_resume(struct platform_device *dev);
#endif

/*******************************************************************************
 * Structures
 *******************************************************************************/
static struct jogball_keytype jog_def[JOGBALL_KEY_NUM] =
{
    /*GPIO                    NAME   Virtual KEY*/
    {JOGBALL_KEY_GPIO_ENTER, "MENU",  KEY_MENU},
    {JOGBALL_KEY_GPIO_UP,    "UP"   , KEY_UP},
    {JOGBALL_KEY_GPIO_DOWN,  "DOWN" , KEY_DOWN},
    {JOGBALL_KEY_GPIO_LEFT,  "LEFT" , KEY_LEFT},
    {JOGBALL_KEY_GPIO_RIGHT, "RIGHT", KEY_RIGHT},
    //{JOGBALL_KEY_GPIO_RIGHT, "RIGHT", KEY_RIGHT},
};

static struct platform_device jogball_device = {
	.name = "jogball_driver",
};

static struct platform_driver jogball_driver = {
#ifdef CONFIG_PM
	.suspend	= jogball_suspend,
	.resume		= jogball_resume,
#endif
	.driver		= {
		.name	= "jogball_driver",
	},
};

/*******************************************************************************
 * Functions
 *******************************************************************************/
#ifdef CONFIG_PM
static enum hrtimer_restart jogball_resume_func(struct hrtimer *timer)
{
    jb_printk("[JOGBALL] jogball_is_suspend = 0\n");
    jogball_is_suspend = 0;
    return HRTIMER_NORESTART;
}

static int jogball_suspend(struct platform_device *dev, pm_message_t state)
{
    int i;
    int rc;
    struct vreg *vreg_jogball;

    jb_printk("[JOGBALL] jogball suspend \n");
    jogball_is_suspend = 1;
    vreg_jogball = vreg_get(NULL, "synt");

    /*suspend... disable IRQ */
    #if JOG_PUSH
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    #else
    for (i = 1; i < JOGBALL_KEY_NUM; i++)
    #endif
    {
        disable_irq(MSM_TO_IRQ(jog_def[i].gpio_num));
    }
    /*suspend... close VREG_SYNT */
    rc = vreg_disable(vreg_jogball);
    if (rc)
    {
        printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc);
    }
    /*suspend... set GPIO to Input mode*/
    #if JOG_PUSH
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    #else
    for (i = 1; i < JOGBALL_KEY_NUM; i++)
    #endif
    {
        rc = gpio_direction_input(jog_def[i].gpio_num);
        if(rc < 0)
        {
            printk("fail to set direction of jogball gpio (%s)! error = %d\n",jog_def[i].name, rc);
        }
    }
    return 0;
}

static int jogball_resume(struct platform_device *dev)
{
    int i;
    int rc;
    struct vreg *vreg_jogball;
    jb_printk("[JOGBALL] jogball resume \n");
    vreg_jogball = vreg_get(NULL, "synt");
    rc = vreg_set_level(vreg_jogball,2600);
    /*resume... set GPIO to Input mode*/
    #if JOG_PUSH
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    #else
    for (i = 1; i < JOGBALL_KEY_NUM; i++)
    #endif
    {
        rc = gpio_direction_input(jog_def[i].gpio_num);
        if(rc < 0)
        {
            printk("fail to set direction of jogball gpio (%s)! error = %d\n",jog_def[i].name, rc);
        }
    }
    /*resume... open VREG_SYNT */
    rc = vreg_enable(vreg_jogball);
    if (rc)
    {
        printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc);
    }
    /*resume... enable IRQ */
    #if JOG_PUSH
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    #else
    for (i = 1; i < JOGBALL_KEY_NUM; i++)
    #endif
    {
        enable_irq(MSM_TO_IRQ(jog_def[i].gpio_num));
    }

    /*the timer is used to avoid unexpected interrupts when power up*/
    //jogball_is_suspend = 0;
    hrtimer_start(&timer_suspend, ktime_set(0, 200*1000000L), HRTIMER_MODE_REL);
    return 0;
}
#endif

static int jogball_gpio_init(void)
{
    int rc = 0;
    int i;
    #if JOG_PUSH
    for(i = 0; i < JOGBALL_KEY_NUM; i++)
    #else
    for(i = 1; i < JOGBALL_KEY_NUM; i++)
    #endif
    {
        rc = gpio_request(jog_def[i].gpio_num, "cci_kb60_jogball");
        if(rc < 0)
        {
            printk("fail to request gpio for jogball (%s)! error = %d\n",jog_def[i].name, rc);
        }

        rc = gpio_direction_input(jog_def[i].gpio_num);
        if(rc < 0)
        {
            printk("fail to set direction of jogball gpio (%s)! error = %d\n",jog_def[i].name, rc);
        }
    }
    return rc;
}

static enum hrtimer_restart jogball_event_direction_func(struct hrtimer *timer)
{
    int i;
    int jb_max_irq;
    int jb_max_count;
    int jb_weight[JOGBALL_DIRECTION_WAY]
                = {JB_WEIGHT_UP, JB_WEIGHT_DOWN, JB_WEIGHT_LEFT, JB_WEIGHT_RIGHT};

    /* ignore these interrupts if the max count of each direction is less than 2*/
    for(i = 1, jb_max_count = 0; i < JOGBALL_DIRECTION_WAY; i++)
    {
        if (jb_direction_count[i] >= jb_direction_count[jb_max_count])
        {
            jb_max_count = i;
        }
    }
    if (jb_direction_count[jb_max_count] < 2)
    {
        jb_printk("[JOGBALL] -------jogball detection event ignore --too less samples\n");
        timer_is_start=0;
        return HRTIMER_NORESTART;
    }

    /* add the weight for 4 way */
    for (i = 0; i < JOGBALL_DIRECTION_WAY; i++)
    {
        jb_direction_count[i] *= jb_weight[i];
    }

    /* find the max direction of 4-way */
    for(i = 1, jb_max_irq = 0; i < JOGBALL_DIRECTION_WAY; i++)
    {
        /* priority of 4-way: right>left>down>up*/
        if (jb_direction_count[i] >= jb_direction_count[jb_max_irq])
        {
            jb_max_irq = i;
        }
    }

    /* if the max direction count is more than THRESHOLD, then send key event */
    /* if not, ignore these interrupts */
    if(jb_direction_count[jb_max_irq] >= JB_EVENT_THRESHOLD)
    {
        input_report_key(jdev,jog_def[jb_max_irq+1].input_key ,1);

        jb_printk("[JOGBALL] -------jogball detection event -------%s %d,%d,%d,%d\n",
                jog_def[jb_max_irq+1].name,
                jb_direction_count[0],
                jb_direction_count[1],
                jb_direction_count[2],
                jb_direction_count[3]);

        input_report_key(jdev,jog_def[jb_max_irq+1].input_key ,0);
    }
    else
    {
        jb_printk("[JOGBALL] -------jogball detection event ignore --%d,%d,%d,%d\n",
                jb_direction_count[0],
                jb_direction_count[1],
                jb_direction_count[2],
                jb_direction_count[3]);
    }

    timer_is_start=0;
    return HRTIMER_NORESTART;
}

static irqreturn_t jogball_irqhandler_direction_key(int irq, void *dev_id)
{
    int i;
    int jog_key_index;

    if (!jogball_is_suspend)
    {
        /* disable four IRQs */
        for(i = JBKEY_UP; i <= JBKEY_RIGHT; i++)
        {
            disable_irq(MSM_TO_IRQ(jog_def[i].gpio_num));
        }
        /* check the direction of this IRQ */
        for(i = JBKEY_UP, jog_key_index = -1; i <= JBKEY_RIGHT; i++)
        {
            if (jog_def[i].gpio_num == IRQ_TO_MSM(irq))
            {
                jog_key_index = i;
                break;
            }
        }
        if (jog_key_index != -1)
        {
            /* if there's no timer starting, reset the counters and start the timer */
            if (!timer_is_start)
            {
                for (i = 0; i < JOGBALL_DIRECTION_WAY; i++)
                {
                    jb_direction_count[i] = 0;
                }

                hrtimer_start(&timer, ktime_set(0, DIRECTION_KEY_SAMPLING_RATE), HRTIMER_MODE_REL);
                timer_is_start = 1;
            }
            /* add this interrupt to the counter */
            jb_direction_count[jog_key_index - 1]++;
            jb_printk("[JOGBALL] -----jogball detection %s\n",jog_def[jog_key_index].name);
        }

        /* enable four IRQs */
        for(i = JBKEY_UP; i <= JBKEY_RIGHT; i++)
        {
            enable_irq(MSM_TO_IRQ(jog_def[i].gpio_num));
        }
    }
    return IRQ_HANDLED;
}

#if JOG_PUSH
static irqreturn_t jogball_irqhandler_enter_key(int irq, void *dev_id)
{
    int key_state;
    if (!jogball_is_suspend)
    {
        disable_irq(irq);
        key_state = jogball_enter_state;
        /* check enter key state that pressed or released */
        key_state = gpio_get_value(JOGBALL_KEY_GPIO_ENTER)?0:1;

        /* if the key state is not equal the original state, update it to input system */
        if (key_state != jogball_enter_state)
        {
            input_report_key(jdev,jog_def[JBKEY_ENTER].input_key ,key_state);
            jogball_enter_state = key_state;
            jb_printk("[JOGBALL] -----------jogball detection %s-%d\n",jog_def[JBKEY_ENTER].name, key_state);
        }
        else
        {
            jb_printk("[JOGBALL] -----------jogball detection %s fail\n",jog_def[JBKEY_ENTER].name);
        }

        enable_irq(irq);
    }
    return IRQ_HANDLED;
}
#endif
static int jogball_input_sys_init(void)
{
    int i;
    int res;
    jdev = input_allocate_device();
    jdev->name = "Jogball";

    set_bit(EV_KEY, jdev->evbit);
    for(i = 0; i < JOGBALL_KEY_NUM; i++)
    {
        set_bit(jog_def[i].input_key, jdev->keybit);
    }

    res = input_register_device(jdev);
    if (res < 0)
    {
        jb_printk("[JOGBALL] register to input subsystem fail! err = %d. \n",res);
    }
    return res;
}

static void jogball_input_sys_exit(void)
{
    input_unregister_device(jdev);
    input_free_device(jdev);
}

static int jogball_irq_init(void)
{
    unsigned long irqflags = 0;
    int err=0;
    int i;
    irqflags = IRQF_TRIGGER_RISING |IRQF_TRIGGER_FALLING;
    #if JOG_PUSH
    err=request_irq(MSM_GPIO_TO_INT(jog_def[JBKEY_ENTER].gpio_num), jogball_irqhandler_enter_key, irqflags, "cci jogball enter key", NULL);

    if(err < 0)
    {
        printk("error to request a irq for jogball!(%s) error=%d\n", jog_def[JBKEY_ENTER].name, err);
        return err;
    }
    else
    {
        jb_printk("[JOGBALL] enter key irq init OK\n");
    }
    #endif
    for(i = 1; i < JOGBALL_KEY_NUM; i++)
    {
        err=request_irq(MSM_GPIO_TO_INT(jog_def[i].gpio_num), jogball_irqhandler_direction_key, irqflags, "cci jogball direction key", NULL);

        if(err < 0)
        {
            printk("error to request a irq for jogball!(%s) error=%d\n", jog_def[i].name, err);
            return err;
        }
        else
        {
            jb_printk("[JOGBALL] direction key irq init OK\n");
        }
    }
    return err;
}

static void jogball_irq_exit(void)
{
    int i;
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    {
        free_irq(MSM_GPIO_TO_INT(jog_def[i].gpio_num), NULL);
    }
}

static void jogball_gpio_exit(void)
{
    int i;
    for (i = 0; i < JOGBALL_KEY_NUM; i++)
    {
        gpio_free(jog_def[i].gpio_num);
    }
}

#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
extern int HW_ID4;
#endif
static int __init cci_mb60_jogball_init(void)
{
    struct vreg *vreg_jogball;
    int rc;

    jb_printk("[JOGBALL] jogball power on \n");
#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
    if (HW_ID4 == 0) {
    	jog_def[JBKEY_RIGHT].gpio_num = JOGBALL_KEY_GPIO_RIGHT;
    	printk(KERN_INFO "%s:  default KEY_RIGHT gpio\n", __func__);
    } else {
    	jog_def[JBKEY_RIGHT].gpio_num = JOGBALL_KEY_GPIO_RIGHT_SPRINT;
    	printk(KERN_INFO "%s:  Sprint KEY_RIGHT gpio\n", __func__);;
    }
#else
    jog_def[JBKEY_RIGHT].gpio_num = JOGBALL_KEY_GPIO_RIGHT;
    printk(KERN_INFO "%s:  default KEY_RIGHT gpio (no HW_ID4)\n", __func__);
#endif

    vreg_jogball = vreg_get(NULL, "synt");
    rc = vreg_set_level(vreg_jogball,2600);
    if (rc)
    {
        printk(KERN_ERR "%s: vreg set level failed (%d)\n", __func__, rc);
    }
    rc = vreg_enable(vreg_jogball);
    if (rc)
    {
        printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc);
    }

    jogball_input_sys_init();
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer.function = jogball_event_direction_func;
#ifdef CONFIG_PM
    hrtimer_init(&timer_suspend, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer_suspend.function = jogball_resume_func;
#endif
    jogball_gpio_init();
    jogball_irq_init();

    platform_device_register(&jogball_device);
    return platform_driver_register(&jogball_driver);
}

static void __exit cci_mb60_jogball_exit(void)
{
    printk("Jogball Uninit\n");
    jogball_input_sys_exit();
    jogball_irq_exit();
    jogball_gpio_exit();
    return;
}


module_init(cci_mb60_jogball_init);
module_exit(cci_mb60_jogball_exit);

MODULE_DESCRIPTION("Track Ball Driver on KB60");
MODULE_AUTHOR("Johnny Lee");
MODULE_LICENSE("GPL v2");

