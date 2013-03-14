/* driver/cci/lightsensors/lightsensor.c
 *
 * Copyright (C) 2009 CCI, Inc.
 * Author:	Ahung.Cheng
 * Date:	2009.5.13
 *
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
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

#include <mach/vreg.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#include "lightsensor.h"

/* /proc/lightsensor */
static struct proc_dir_entry *lightsensor_proc_dir = NULL;
struct wake_lock lightsensor_idle_wake_lock;

/* Lock */
//static struct mutex lightsensor_lock;
static DEFINE_MUTEX(lightsensor_lock);
static int BacklightState = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend LT_ely_suspend;
#endif

extern int DVT_1_3;
extern int DVT_2;



/*******************************************************************************
 * * Local Function Declaration
 * *******************************************************************************/

static void Enable_vreg_gp5(void)
{
	int rc;

	struct vreg *vreg_gp5 = NULL;

	vreg_gp5 = vreg_get(0, "gp5");

	if(BacklightState == 0)
		return;

	if(DVT_2) {
		printk("%s HW VERSION : DVT_2\n",__func__);
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
	else
		printk("%s HW VERSION : before DVT_2\n",__func__);
}


static void Disable_vreg_gp5(void)
{
	int rc;
	struct vreg *vreg_gp5 = NULL;

	rc = 0;
	vreg_gp5 = vreg_get(0, "gp5");

	if(BacklightState == 0)
		return;

	if(DVT_2) {
		printk("%s HW VERSION : DVT_2\n",__func__);

		rc = vreg_disable(vreg_gp5);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable gp5 failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
	}
	else
		printk("%s HW VERSION : before DVT_2\n",__func__);
}



#ifdef CONFIG_HAS_EARLYSUSPEND
void lightsensor_early_suspend(struct early_suspend *h)
{
	Disable_vreg_gp5();
}

void lightsensor_later_resume(struct early_suspend *h)
{
	Enable_vreg_gp5();
}
#endif


/*******************************************************************************
 * * External Variable Declaration
 * *******************************************************************************/

/*******************************************************************************
 * * External Function Declaration
 * *******************************************************************************/
static int getLightsensor_cci_rpc(void)
{
	int rpc_id;
	struct msm_rpc_endpoint *ep = NULL;
	int lightsensor_data;
	struct cci_rpc_lig_sen_req  req;
	struct cci_rpc_lig_sen_rep  rep;

	wake_lock(&lightsensor_idle_wake_lock );

	ep = msm_rpc_connect(CCIPROG, CCIVERS, 0);
	if (IS_ERR(ep))
	{
		printk(KERN_ERR "%s: [LIGHTSENSOR] init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}
	
	memset(&rep, 0, sizeof(rep));
	rpc_id = msm_rpc_call_reply(ep, ONCRPC_CCI_READ_LIGHT_SENSOR,
				&req, sizeof(req),
				&rep, sizeof(rep),
				5 * HZ);

	if (rpc_id < 0)
	{
		printk(KERN_ERR "[LIGHTSENSOR]: Can't select MSM device");
		msm_rpc_close(ep);
		return rpc_id; 
	}

	lightsensor_data = (char)be32_to_cpu(rep.err_flag);
	
	// close
	msm_rpc_close(ep);

	wake_unlock(&lightsensor_idle_wake_lock );

	//printk("[LIGHTSENSOR] : Light level = %d\n",lightsensor_data);

	return lightsensor_data;
}


static int
proc_lightsensor_data_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	int len = 0; 

	mutex_lock(&lightsensor_lock);
	len += sprintf(len + page, "%d\n", getLightsensor_cci_rpc());
	mutex_unlock(&lightsensor_lock);
	return len;
}

#if 1
static int
proc_lightsensor_data_write(struct file *file,
			const char __user * buffer,
			unsigned long count, void *data)
{
	char mode = '0';

	if(count > 1){
		printk(KERN_ERR "%s data_len=%ld > 2\n",__func__,count);
		return -EINVAL;
	}

	//printk("lightsensor count = %ld\n",count);

	if( copy_from_user(&mode, buffer, count) ) {
		printk(KERN_ERR "%s:%s():%d --> Fail to copy_from_user()\n", __FILE__, __func__, __LINE__);
		return 0;
	}

	//printk("lightsensor mode = %c\n",mode);

	switch (mode) {

		case '0':

			if(BacklightState == 1){
				Disable_vreg_gp5();
				BacklightState = 0;
				//printk("lightsensor early_suspend\n");
			}
			break;

		case '1':

			if(BacklightState == 0){
				BacklightState = 1;
				Enable_vreg_gp5();
				//printk("lightsensor later_resume\n");
			}
			break;

		default:
			printk(KERN_ERR "%s mode ERROR\n",__func__);
			return -EINVAL;
	}
	return count;

}
#endif

static int init_proc(void)
{

	//struct proc_dir_entry *lightsensor_proc_enable = NULL;
	struct proc_dir_entry *lightsensor_proc_data = NULL;
	int retval = 0;
	
	/* Create /proc/lightsensor */
	if( !(lightsensor_proc_dir = proc_mkdir("lightsensor", NULL)) ) {
		printk(KERN_ERR "[LIGHTSENSOR]: %s:%s():%d --> Cannot create /proc/lightsensor\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		//lightsensor_proc_dir->owner = THIS_MODULE;
	}
	
	/* create data */
	if( !(lightsensor_proc_data = create_proc_entry("data", 0666, lightsensor_proc_dir)) ) {
		printk(KERN_ERR "[LIGHTSENSOR]: %s:%s():%d --> Cannot create /proc/lightsensor/data\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		lightsensor_proc_data->read_proc = proc_lightsensor_data_read;
		#if 1
		lightsensor_proc_data->write_proc = proc_lightsensor_data_write;
		#endif
		//lightsensor_proc_data->owner = THIS_MODULE;
	}
	
end:
	return retval;	
}

static int __init lightsensor_init(void)
{
	int retval;

	printk(KERN_INFO"[LIGHTSENSOR]: Lightsensor driver has been init.\n");
	wake_lock_init(&lightsensor_idle_wake_lock, WAKE_LOCK_IDLE, "lightsensor");

	//mutex_init(&lightsensor_lock);

	retval = init_proc();

	Enable_vreg_gp5();

#ifdef CONFIG_HAS_EARLYSUSPEND
	LT_ely_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	LT_ely_suspend.suspend = lightsensor_early_suspend;
	LT_ely_suspend.resume = lightsensor_later_resume;
	register_early_suspend(&LT_ely_suspend);
#endif

	return 0;
}

static void cleanup_proc(void)
{
	remove_proc_entry("data", lightsensor_proc_dir);
	remove_proc_entry("lightsensor", NULL);

	return;
}


static void __exit lightsensor_exit(void)
{	
	printk(KERN_INFO "[LIGHTSENSOR]: Lightsensor driver has been unloaded.\n");
	wake_lock_destroy(&lightsensor_idle_wake_lock);
	cleanup_proc();
	return;
}

module_init(lightsensor_init);
module_exit(lightsensor_exit);

MODULE_LICENSE("GPL v2");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Aaron_Chen & Ahung_Cheng & Jimmy_Yen");
