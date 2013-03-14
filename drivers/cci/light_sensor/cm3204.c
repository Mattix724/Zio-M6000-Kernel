/*
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
//HuaPu / 20101111 / refine this driver code
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>
#include <linux/proc_fs.h>
#include <asm/atomic.h>
#include <mach/vreg.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
/* for debugging */
#define DEBUG 0

#define LOG_TAG "CM3204"
#define SENSOR_NAME "light"
//HuaPu: begin /20101210 / BugID:1070 [KB62][Froyo][light-sensor]Abnormal brightness changes fixing and refine the delay time of light sensor
#define SENSOR_DEFAULT_DELAY            (500)   /* 500 ms */
//HuaPu: end
#define SENSOR_MAX_DELAY                (2000)  /* 2000 ms */
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)

#define CCIPROG	0x30001000
#define CCIVERS	0x00010001

#define MSM_SHUTDOWN_LIGHT_SENSOR_GPIO  32

#define ONCRPC_CCI_READ_LIGHT_SENSOR 27

#define LIGHTSENSOR_POWER_ON 0
#define LIGHTSENSOR_POWER_OFF 1

#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)

//HuaPu: for debugging
#define debugFlag 1


#define earlySuspend 1
extern int DVT_1_3;
extern int DVT_2;
static bool suspendStatus = false;
static int readCount = 0;

//settings>display>brightness>autobacklight status
static bool Settings_status=false;

#if earlySuspend
static void cm3204_early_suspend(struct early_suspend *h);
static void cm3204_late_resume(struct early_suspend *h);
#endif
static int suspend(void);
static int resume(void);

struct sensor_data {
    struct mutex mutex;
    atomic_t enabled;
    atomic_t delay;
    atomic_t last;
    struct mutex data_mutex;
    struct input_dev *input;
    struct delayed_work work;
#if DEBUG
    int suspend;
#endif
};

#if earlySuspend
static struct early_suspend early_suspend;
#endif

struct cci_rpc_lig_sen_req {
	struct rpc_request_hdr hdr;
};

struct cci_rpc_lig_sen_rep {
	struct rpc_reply_hdr hdr;
	uint32_t err_flag;
};

static struct platform_device *sensor_pdev = NULL;
static struct input_dev *this_data = NULL;

static int getLightsensor_cci_rpc(void)
{
	int rpc_id;
	struct msm_rpc_endpoint *ep = NULL;
	int lightsensor_data;
	struct cci_rpc_lig_sen_req  req;
	struct cci_rpc_lig_sen_rep  rep;	

	ep = msm_rpc_connect(CCIPROG, CCIVERS, 0);
	if (IS_ERR(ep))
	{
		printk(KERN_ERR "[%s]%s: init rpc failed! rc = %ld\n", LOG_TAG, __FUNCTION__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}
	
	memset(&rep, 0, sizeof(rep));
	rpc_id = msm_rpc_call_reply(ep, ONCRPC_CCI_READ_LIGHT_SENSOR,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);

	if (rpc_id < 0)
	{
		printk(KERN_ERR "[%s]%s: Can't select MSM device(rc=%d)\n", LOG_TAG, __FUNCTION__, rpc_id);
		msm_rpc_close(ep);
		return rpc_id; 
	}
	lightsensor_data = (char)be32_to_cpu(rep.err_flag);
	msm_rpc_close(ep);	

	return lightsensor_data;
}

static void cm3204_work_func(struct work_struct *work)
{
    struct sensor_data *data = container_of((struct delayed_work *)work, struct sensor_data, work);
    int value;
    int adj = 1000;  //1000*(514/255.000);
    int delay = atomic_read(&data->delay);
//HuaPu: begin /20101210 / BugID:1070 [KB62][Froyo][light-sensor]Abnormal brightness changes fixing and refine the delay time of light sensor
    if(delay < SENSOR_DEFAULT_DELAY)
		delay = SENSOR_DEFAULT_DELAY;
//HuaPu: end

    //unsigned long delay_jiff = delay_to_jiffies(delay)+1;
    value = adj*getLightsensor_cci_rpc();
#if debugFlag
	if(readCount == 100){
		printk(KERN_INFO "[light-sensor]>>>HuaPu>>>value:%d\n", value);
		readCount = 0;
	}
	else
		readCount++;
#endif

    input_report_abs(data->input, ABS_X, value);	
    input_sync(data->input);
    atomic_set(&data->last, value);
    schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
}

static void Enable_vreg_gp5(void)
{
	int rc;

	struct vreg *vreg_gp5 = NULL;

	vreg_gp5 = vreg_get(0, "gp5");

	/*if(BacklightState == 0)
		return;*/


		rc = vreg_set_level(vreg_gp5, 2800);
		if (rc) {
			printk(KERN_ERR "%s: vreg set gp5 level failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
		rc = vreg_enable(vreg_gp5);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable gp5 failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}


}
static void Disable_vreg_gp5(void)
{
	int rc;
	struct vreg *vreg_gp5 = NULL;

	rc = 0;
	vreg_gp5 = vreg_get(0, "gp5");

	/*if(BacklightState == 0)
		return;*/


		rc = vreg_disable(vreg_gp5);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable gp5 failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
	}

static int
suspend(void)
{
    /* implement suspend of the sensor */
	struct sensor_data *data = input_get_drvdata(this_data);
	if(Settings_status){
	if (atomic_cmpxchg(&data->enabled, 1, 0)) {
	   cancel_delayed_work_sync(&data->work);
        }
	}
	Disable_vreg_gp5();

    return 0;
}

static int
resume(void)
{
    /* implement resume of the sensor */
    printk(KERN_ERR "[%s]%s\n", LOG_TAG, __FUNCTION__);
	struct sensor_data *data = input_get_drvdata(this_data);
	if(Settings_status){
		Enable_vreg_gp5();
		if (!atomic_cmpxchg(&data->enabled, 0, 1)) {
		int delay = atomic_read(&data->delay);

	   schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
		}
	}
#if DEBUG
    {
        struct sensor_data *data = input_get_drvdata(this_data);
        data->suspend = 0;
    }
#endif /* DEBUG */

    return 0;
}


/* Sysfs interface */
static ssize_t
sensor_delay_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);
    int delay;   

    delay = atomic_read(&data->delay);
	
    return sprintf(buf, "%d\n", delay);
}

static ssize_t
sensor_delay_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);
    int value = simple_strtoul(buf, NULL, 10);
    int enabled;  
    int delay;	
    atomic_read(&data->enabled);
    atomic_read(&data->delay);

    if (value < 0) {
	printk(KERN_ERR	"[%s]%s: value<0 Fail\n", LOG_TAG, __FUNCTION__);
        return count;
    }

    if (SENSOR_MAX_DELAY < value) {
        value = SENSOR_MAX_DELAY;
    }

    mutex_lock(&data->mutex);

    atomic_set(&data->delay, value);
    enabled = atomic_read(&data->enabled);
    delay = atomic_read(&data->delay);
    if (enabled) {
	cancel_delayed_work_sync(&data->work);
	schedule_delayed_work(&data->work, msecs_to_jiffies(delay) + 1);
    }
	
    mutex_unlock(&data->mutex);

    return count;
}

static ssize_t
sensor_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);
    int enabled;    

	enabled = atomic_read(&data->enabled);

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t
sensor_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);
    int value = simple_strtoul(buf, NULL, 10);
    int enabled = atomic_read(&data->enabled);
    int delay = atomic_read(&data->delay);

    if (value != 0 && value != 1) {
	 printk(KERN_ERR "[%s]%s: return - invalid enable value(%d)\n", 
	 	LOG_TAG, __FUNCTION__, value);
        return count;
    }

    mutex_lock(&data->mutex);

    delay = atomic_read(&data->delay);
    if(!suspendStatus){
	if (value)  { /* Set to Enable */
		Enable_vreg_gp5();
		if (!atomic_cmpxchg(&data->enabled, 0, 1)) {
			delay = atomic_read(&data->delay);
			schedule_delayed_work(&data->work, msecs_to_jiffies(delay)+1);
		}
	} else{ /* Set to Disable */
	if (atomic_cmpxchg(&data->enabled, 1, 0)) {
			cancel_delayed_work_sync(&data->work);
	}
		Disable_vreg_gp5();
	}
   }
   if(value)
		Settings_status=true;  //settings>display>brightness>autobacklight>enable
	else
		Settings_status=false;  //settings>display>brightness>autobacklight>disable

    atomic_set(&data->enabled, value);

    enabled = atomic_read(&data->enabled);
    delay = atomic_read(&data->delay);

    mutex_unlock(&data->mutex);

    return count;
}

static ssize_t
sensor_wake_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    static int cnt = 1;
	
    /*printk(KERN_ERR "[%s]%s\n", LOG_TAG, __FUNCTION__);*/
    input_report_abs(input_data, ABS_WAKE, cnt++);
    return count;
}

static ssize_t
sensor_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    //unsigned long flags;
    int x;
    x = input_data->abs[ABS_X];

    return sprintf(buf, "%d\n", x);
}

static ssize_t
sensor_status_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    //unsigned long flags;
    int status;

    status = input_data->abs[ABS_STATUS];

    return sprintf(buf, "%d\n", status);
}

#if DEBUG

static ssize_t sensor_debug_suspend_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct sensor_data *data = input_get_drvdata(input_data);

    return sprintf(buf, "%d\n", data->suspend);
}

static ssize_t sensor_debug_suspend_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
    unsigned long value = simple_strtoul(buf, NULL, 10);

    if (value) {
        suspend();
    } else {
        resume();
    }

    return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
        sensor_delay_show, sensor_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        sensor_enable_show, sensor_enable_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,
        NULL, sensor_wake_store);
static DEVICE_ATTR(data, S_IRUGO, sensor_data_show, NULL);
static DEVICE_ATTR(status, S_IRUGO, sensor_status_show, NULL);

#if DEBUG
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
                   sensor_debug_suspend_show, sensor_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *sensor_attributes[] = {
    &dev_attr_delay.attr,
    &dev_attr_enable.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
    &dev_attr_status.attr,
#if DEBUG
    &dev_attr_debug_suspend.attr,
#endif /* DEBUG */
    NULL
};

static struct attribute_group sensor_attribute_group = {
    .attrs = sensor_attributes
};

#if earlySuspend
static void cm3204_early_suspend(struct early_suspend *h)
{
	//printk(KERN_INFO "[cm3204.c]HuaPu>>>cm3204_early_suspend\n");
	suspendStatus = true;
	suspend();

}
static void cm3204_late_resume(struct early_suspend *h)
{
	//printk(KERN_INFO "[cm3204.c]HuaPu>>>cm3204_late_resume\n");
	suspendStatus = false;
	resume();
}
#endif
/*static int
sensor_suspend(struct platform_device *pdev, pm_message_t state)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int rt = 0;

    mutex_lock(&data->mutex);    
	
    if (!atomic_read(&data->enabled)) {
        rt = suspend();
    }

    mutex_unlock(&data->mutex);

	//motoko
	printk(KERN_INFO "motoko>>>cm3204_suspend\n");
    return rt;
}*/

/*static int
sensor_resume(struct platform_device *pdev)
{
    struct sensor_data *data = input_get_drvdata(this_data);
    int rt = 0;

    mutex_lock(&data->mutex);    
	
    if (atomic_read(&data->enabled)) {
        rt = resume();
    }

    mutex_unlock(&data->mutex);

	//motoko
	printk(KERN_INFO "motoko>>>cm3204_resume\n");
    return rt;
}*/
static int
sensor_probe(struct platform_device *pdev)
{
    struct sensor_data *data = NULL;
    struct input_dev *input_data = NULL;
    int input_registered = 0, sysfs_created = 0;
    int rt;
#if 0
    printk(KERN_INFO "[cm3204]>>>HuaPu>>>sensor_probe\n");
#endif
    data = kzalloc(sizeof(struct sensor_data), GFP_KERNEL);
    if (!data) {
        rt = -ENOMEM;
        goto err;
    }
	
    mutex_init(&data->mutex);    
    mutex_init(&data->data_mutex);
    INIT_DELAYED_WORK(&data->work, cm3204_work_func);
	
    atomic_set(&data->enabled, 0);
    atomic_set(&data->delay, SENSOR_DEFAULT_DELAY);
    atomic_set(&data->last, 0);

    input_data = input_allocate_device();
    if (!input_data) {
        rt = -ENOMEM;
        printk(KERN_ERR "[%s]%s: Failed to allocate input_data device\n", LOG_TAG, __FUNCTION__);
        goto err;
    }

    set_bit(EV_ABS, input_data->evbit);
    input_set_capability(input_data, EV_ABS, ABS_X);
    //input_set_capability(input_data, EV_ABS, ABS_STATUS); /* status */
    //input_set_capability(input_data, EV_ABS, ABS_WAKE); /* wake */
    //input_set_capability(input_data, EV_ABS, ABS_CONTROL_REPORT); /* enabled/delay */
    input_data->name = SENSOR_NAME;

    rt = input_register_device(input_data);
    if (rt) {
        printk(KERN_ERR "[%s]%s: Unable to register input_data device: %s\n", 
	     LOG_TAG, __FUNCTION__, input_data->name);
        goto err;
    }
    input_set_drvdata(input_data, data);
    input_registered = 1;

    rt = sysfs_create_group(&input_data->dev.kobj,
            &sensor_attribute_group);
    if (rt) {
        printk(KERN_ERR
               "[%s]%s: sysfs_create_group failed[%s]\n",
               LOG_TAG, __FUNCTION__, input_data->name);
        goto err;
    }
    sysfs_created = 1;

    this_data = input_data;
    data->input = input_data;

#if earlySuspend
	early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	early_suspend.suspend = cm3204_early_suspend;
	early_suspend.resume = cm3204_late_resume;
	register_early_suspend(&early_suspend);
#endif
   
    return 0;

err:
    if (data != NULL) {
        if (input_data != NULL) {
            if (sysfs_created) {
                sysfs_remove_group(&input_data->dev.kobj,
                        &sensor_attribute_group);
            }
            if (input_registered) {
                input_unregister_device(input_data);
            }
            else {
                input_free_device(input_data);
            }
            input_data = NULL;
        }
        kfree(data);
    }
    printk(KERN_ERR "[%s]%s - FAIL!\n", LOG_TAG, __FUNCTION__);
    return rt;
}

static int
sensor_remove(struct platform_device *pdev)
{
    struct sensor_data *data;
	
    printk(KERN_ERR "[%s]%s\n", LOG_TAG, __FUNCTION__);
	
    if (this_data != NULL) {
        data = input_get_drvdata(this_data);
        sysfs_remove_group(&this_data->dev.kobj,
                &sensor_attribute_group);
        input_unregister_device(this_data);

#if earlySuspend
	unregister_early_suspend(&early_suspend);
#endif
        if (data != NULL) {
            kfree(data);
        }
    }

    return 0;
}

/*
 * Module init and exit
 */
static struct platform_driver sensor_driver = {
    .probe      = sensor_probe,
    .remove     = sensor_remove,
    //.suspend    = sensor_suspend,
    //.resume     = sensor_resume,
    .driver = {
        .name   = SENSOR_NAME,
        .owner  = THIS_MODULE,
    },
};

static int __init sensor_init(void)
{
    printk(KERN_ERR "[%s]%s\n", LOG_TAG, __FUNCTION__);
	
    sensor_pdev = platform_device_register_simple(SENSOR_NAME, 0, NULL, 0);
    if (IS_ERR(sensor_pdev)) {
	 printk(KERN_ERR "[%s]%s - device register FAIL!\n", LOG_TAG, __FUNCTION__);
        return -1;
    }

      int rc;
      rc = platform_driver_register(&sensor_driver);
      if (rc < 0) {
         printk(KERN_ERR "[%s]%s - driver register FAIL(%d)!\n", LOG_TAG, __FUNCTION__, rc);         
      }
	  
      return rc;	  

}
module_init(sensor_init);

static void __exit sensor_exit(void)
{
    printk(KERN_ERR "[%s]%s\n", LOG_TAG, __FUNCTION__);
	
    platform_driver_unregister(&sensor_driver);
    platform_device_unregister(sensor_pdev);
}
module_exit(sensor_exit);

MODULE_AUTHOR("Yamaha Corporation");
MODULE_LICENSE( "GPL" );
MODULE_VERSION("1.1.0");
