/*
 *
 * Copyright (C) 2009 CCI, Inc.
 * Author:	Erix Cheng
 * Date:	2009.8.14
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
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/rtc.h>

#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>

#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>

#include "vibrator.h"

MODULE_LICENSE("GPL v2");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Erix Cheng");

/*******************************************************************************
 * * Local Variable Declaration
 * *******************************************************************************/
/* /proc/vibrator */
static struct proc_dir_entry *vibrator_proc_dir = NULL;
static signed long prev_time_val = -1;

/* Lock */
static struct mutex vibrator_lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend early_suspend;
#endif

/*******************************************************************************
 * * Local Function Declaration
 * *******************************************************************************/

/*******************************************************************************
 * * External Variable Declaration
 * *******************************************************************************/
extern struct proc_dir_entry proc_root;
/*******************************************************************************
 * * External Function Declaration
 * *******************************************************************************/

/*** Functions ***/
int call_vibrator_rpc(int timeout)
{
	int rpc_id=0;	
	struct msm_rpc_endpoint *ep = NULL;
	struct cci_rpc_vib_on_req req_on;
	struct cci_rpc_vib_off_req req_off;

	printk("Start to call Vibrator RPC for %d msec.\n", timeout);
        //init RPC
        ep = msm_rpc_connect(CCIPROG, CCIVERS, 0);
        if (IS_ERR(ep))
        {
                printk(KERN_ERR "%s: init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
                return PTR_ERR(ep);
         }

	 if (timeout == 0){
		rpc_id = msm_rpc_call(ep, ONCRPC_CCI_VIB_OFF, &req_off, sizeof(req_off) , 5 * HZ);
		if (rpc_id < 0)
         	{
                	printk("Vibration process STOP ... Fail\n");
                	msm_rpc_close(ep);
			return -1;
		}
	 }
	 else{
		req_on.duration= cpu_to_be32(timeout*1000);
		rpc_id = msm_rpc_call(ep, ONCRPC_CCI_VIB, &req_on, sizeof(req_on) , 5 * HZ);
		if (rpc_id < 0)
		{
                	printk("Vibration process START ... Fail\n");
                	msm_rpc_close(ep);
                	return -1;
         	}
	 }

	  msm_rpc_close(ep);
	  return 1;
}

static int
proc_vibrator_enable_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	return 0;
}

static int
proc_vibrator_enable_write(struct file *file, const char *buffer,
		                   unsigned long count, void *data)
{
	int ret = 0 , mutex_ret = 0;
	signed long timeout = 0;
	char bufstr[64], *endp;

	mutex_ret = mutex_trylock(&vibrator_lock);
	//printk("mutex lock status: %i\n", mutex_ret );

	if(mutex_ret !=1){
	//The vibrator is already used , can't get control , return.
		return EBUSY;
	}

	printk ("*** %s ***\n", __func__);

	bufstr[0] = '\0';
	ret = copy_from_user(&bufstr, buffer, (sizeof(bufstr)-1));

	if (ret) {
		printk(KERN_ERR "%s:%s():%d --> Fail to copy_from_user()\n", __FILE__, __func__, __LINE__);
		goto end;
	}

	if (count < sizeof(bufstr))
		bufstr[count] = '\0';
	else
		bufstr[sizeof(bufstr)-1] = '\0';

	timeout = simple_strtol(bufstr, &endp, 0);
	if (timeout >= 0) {
		if (!(prev_time_val == 0 && timeout == 0)) {
			/* Prevent send 2 continuous stopping RPC call. */
			prev_time_val = timeout;
			ret = call_vibrator_rpc(timeout);
		}
	} else {
		goto end;
	}

end:
	mutex_unlock(&vibrator_lock);

	return count;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void vibrator_early_suspend(struct early_suspend *h)
{
	call_vibrator_rpc(0);
}

void vibrator_later_resume(struct early_suspend *h)
{
	
}
#endif

void vibrator_register_driver(void)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	early_suspend.suspend = vibrator_early_suspend;
	early_suspend.resume = vibrator_later_resume;
	register_early_suspend(&early_suspend);
#endif
}

static int init_proc(void)
{
	int retval = 0;

	struct proc_dir_entry *vibrator_proc_enable = NULL;
	/* Create /proc/vibrator */
	if( !(vibrator_proc_dir = proc_mkdir("vibrator", &proc_root)) ) {
		printk(KERN_ERR "%s:%s():%d --> Cannot create /proc/vibrator\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		//vibrator_proc_dir->owner = THIS_MODULE;
	}
	/* create enable entry */
	if( !(vibrator_proc_enable = create_proc_entry("enable", 0666, vibrator_proc_dir)) ) {
		printk(KERN_ERR "%s:%s():%d --> Cannot create /proc/vibrator/enable\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		//vibrator_proc_enable->data = 0;
		vibrator_proc_enable->read_proc = proc_vibrator_enable_read;
		vibrator_proc_enable->write_proc = proc_vibrator_enable_write;
		//vibrator_proc_enable->owner = THIS_MODULE;
	}

	vibrator_register_driver();

	return retval;

end:
	return retval;
}

static void cleanup_proc(void)
{
	remove_proc_entry("enable", vibrator_proc_dir);
	remove_proc_entry("vibrator", &proc_root);

	return;
}


static int __init vibrator_init(void)
{
	int retval = 0;

	/* Init Lock */
	mutex_init(&vibrator_lock);
	/* Init Proc */
	retval = init_proc();

	printk(KERN_INFO "Vibrator driver has been loaded.\n");

	return retval;
}

static void __exit vibrator_exit(void)
{	
	/* Destroy Proc */
	cleanup_proc();
	/* Destroy Lock */
	mutex_destroy(&vibrator_lock);

	printk(KERN_INFO "Vibrator driver has been unloaded.\n");

	return;
}

module_init(vibrator_init);
module_exit(vibrator_exit);
