/*
 * Copyright (C) 2009 CCI, Inc.
 * Author:	Johnny Lee
 * Date:	Oct.12, 2009
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*******************************************************************************
*    Includes
********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>

#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/rtc.h>

#include <linux/sched.h>
#include <linux/workqueue.h>

#include "audio_pm.h"

/*******************************************************************************
*    Definitions
********************************************************************************/
#define DRIVER_NAME "pm_speaker"
#define SPK_CMD_DELAY_ON   2
#define SPK_CMD_ON         1
#define SPK_CMD_OFF        0

/*******************************************************************************
*    Local Variable Declaration
********************************************************************************/
static struct proc_dir_entry *speaker_proc_dir = NULL;
static signed long speaker_enable = 0;
//static signed long speaker_gain = 6;
static struct mutex speaker_lock;
static int audmgr_enabled = 0;
static struct delayed_work speaker_check_work;
static struct delayed_work speaker_check_work_off;

/*******************************************************************************
*   Functions
********************************************************************************/
static int speaker_cmd(int cmd)
{
	int retval = 0;

	cancel_delayed_work(&speaker_check_work);
	cancel_delayed_work(&speaker_check_work_off);

	switch (cmd) {
	case SPK_CMD_DELAY_ON:
		printk(KERN_INFO "[CLS]%s SPK_CMD_DELAY_ON\n", __func__);
		retval = pmic_set_speaker_delay(SPKR_DLY_100MS);
		if (retval < 0) goto error_handle;
		retval = pmic_speaker_cmd(SPKR_ON);
		if (retval < 0) goto error_handle;
		retval = pmic_spkr_en_left_chan(1);
		if (retval < 0) goto error_handle;
		break;
	case SPK_CMD_ON:
		/* de-pop noise flag */
	        printk(KERN_INFO "[CLS]%s SPK_CMD_ON - audmgr_enabled=%d\n", __func__, audmgr_enabled);
		if(audmgr_enabled == 1) {
			retval = pmic_set_speaker_delay(SPKR_DLY_10MS);
			if (retval < 0) goto error_handle;
			retval = pmic_speaker_cmd(SPKR_ON);
			if (retval < 0) goto error_handle;
			retval = pmic_spkr_en_left_chan(1);
			if (retval < 0) goto error_handle;
		}
		break;
    case SPK_CMD_OFF:
		printk(KERN_INFO "[CLS]%s SPK_CMD_OFF\n", __func__);
		retval = pmic_speaker_cmd(SPKR_OFF);
		if (retval < 0) goto error_handle;
		retval = pmic_spkr_en_left_chan(0);
		if (retval < 0) goto error_handle;
		break;

	default:
		return -1;
	}

error_handle:
	if (retval < 0)
	{
		if (cmd == SPK_CMD_ON || cmd == SPK_CMD_DELAY_ON)
			schedule_delayed_work(&speaker_check_work, msecs_to_jiffies(600));
		else if (cmd == SPK_CMD_OFF)
			schedule_delayed_work(&speaker_check_work_off, msecs_to_jiffies(600));
	}

	
	return retval;
}

static void speaker_check_func (struct work_struct *no_use)
{
	uint enabled = 0;
	pmic_spkr_is_en(LEFT_SPKR, &enabled);
	if (enabled == 0) {
		printk(KERN_INFO "[CLS]%s Power ON speaker AGAIN!\n", __func__);
		if (pmic_set_speaker_delay(SPKR_DLY_10MS) < 0)
			printk(KERN_INFO "[CLS]%s retry pmic_set_speaker_delay(SPKR_DLY_10MS) failed\n", __func__);
		if (pmic_speaker_cmd(SPKR_ON) < 0)
			printk(KERN_INFO "[CLS]%s retry pmic_speaker_cmd(SPKR_ON) failed\n", __func__);
		if (pmic_spkr_en_left_chan(1) < 0)
			printk(KERN_INFO "[CLS]%s retry pmic_spkr_en_left_chan(1) failed\n", __func__);

	} else {
		printk(KERN_INFO "%s ---Speaker check OK!\n", __func__);
	}
}
static void speaker_check_func_off (struct work_struct *no_use)
{
	uint enabled = 0;
	pmic_spkr_is_en(LEFT_SPKR, &enabled);
	if (enabled == 1) {
		printk(KERN_INFO "[CLS]%s Power OFF speaker AGAIN!\n", __func__);
		if (pmic_speaker_cmd(SPKR_OFF) < 0)
			printk(KERN_INFO "[CLS]%s retry pmic_speaker_cmd(SPKR_OFF) failed\n", __func__);
		if (pmic_spkr_en_left_chan(0) < 0)
			printk(KERN_INFO "[CLS]%s retry pmic_spkr_en_left_chan(0) failed\n", __func__);

	} else {
		printk(KERN_INFO "%s ---Speaker check off OK!\n", __func__);
	}
}


static int
proc_speaker_enable_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	int len = 0; 
	int *timeout = (int *)data;

	mutex_lock(&speaker_lock);

	len += sprintf(len + page, "%d\n", *timeout);

	mutex_unlock(&speaker_lock);

	return len;
}

static int
proc_speaker_enable_write(struct file *file, const char *buffer,
		                   unsigned long count, void *data)
{
	signed long tmp = 0;
	signed long *enable = (signed long *)data;
	char bufstr[64], *ep;

	mutex_lock(&speaker_lock);

	bufstr[0] = '\0';

	if( copy_from_user(&bufstr, buffer, (sizeof(bufstr)-1)) ) {
		printk(KERN_ERR "%s %d --> Fail to copy_from_user()\n", __func__, __LINE__);
		goto end;
	}

	if( count < sizeof(bufstr) ) {
		bufstr[count] = '\0';
	}
	else {
		bufstr[sizeof(bufstr)-1] = '\0';
	}

	tmp = simple_strtol(bufstr, &ep, 0);

	if( tmp == SPK_CMD_ON ) {
		*enable = tmp;
	}
	else {
		*enable = SPK_CMD_OFF;
	}
	/* send command */
	if (speaker_cmd(speaker_enable) < 0)
	{
		printk(KERN_ERR "%s %d --> Fail to turn on speaker\n", __func__, __LINE__);
		count = -1;
	}

end:
	mutex_unlock(&speaker_lock);

	return count;
}

static int
proc_speaker_gain_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	return 0;
}

static int
proc_speaker_gain_write(struct file *file, const char *buffer,
		                   unsigned long count, void *data)
{
	signed long tmp = 0;
	signed long *gain = (signed long *)data;
	char bufstr[64], *ep;

	mutex_lock(&speaker_lock);

	bufstr[0] = '\0';

	if( copy_from_user(&bufstr, buffer, (sizeof(bufstr)-1)) ) {
		printk(KERN_ERR "%s %d --> Fail to copy_from_user()\n", __func__, __LINE__);
		goto end;
	}

	if( count < sizeof(bufstr) ) {
		bufstr[count] = '\0';
	}
	else {
		bufstr[sizeof(bufstr)-1] = '\0';
	}

	tmp = simple_strtol(bufstr, &ep, 0);

	if( tmp >=0 || tmp <=9 ) {
		*gain = tmp;
	}
	else {
		*gain = 0;
	}
	/* send command */
	if (0 == pmic_set_speaker_gain(*gain)) {
		printk(KERN_INFO "pmic_set_speaker_gain success  gain = %d\n",(int)(*gain));
	} else {
	    printk(KERN_INFO "pmic_set_speaker_gain failed\n");
	}

end:
	mutex_unlock(&speaker_lock);

	return count;
}


static int init_proc(void)
{
	int retval = 0;

	struct proc_dir_entry *speaker_proc_enable = NULL;
	struct proc_dir_entry *speaker_proc_gain = NULL;
	/* Create /proc/speaker */
	if( !(speaker_proc_dir = proc_mkdir("speaker", NULL)) ) {
		printk(KERN_ERR "%s %d --> Cannot create /proc/speaker\n", __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		//speaker_proc_dir->owner = THIS_MODULE;
	}
	/* create enable */
	if( !(speaker_proc_enable = create_proc_entry("enable", 0666, speaker_proc_dir)) ) {
		printk(KERN_ERR "%s %d --> Cannot create /proc/speaker/enable\n", __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		speaker_proc_enable->data = &speaker_enable;
		speaker_proc_enable->read_proc = proc_speaker_enable_read;
		speaker_proc_enable->write_proc = proc_speaker_enable_write;
		//speaker_proc_enable->owner = THIS_MODULE;
	}

	/* create gain */
	if( !(speaker_proc_gain = create_proc_entry("gain", 0666, speaker_proc_dir)) ) {
		printk(KERN_ERR "%s %d --> Cannot create /proc/speaker/gain\n", __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		//speaker_proc_gain->data = &speaker_gain;
		speaker_proc_gain->read_proc = proc_speaker_gain_read;
		speaker_proc_gain->write_proc = proc_speaker_gain_write;
		//speaker_proc_gain->owner = THIS_MODULE;
	}

end:
	return retval;
}

static void cleanup_proc(void)
{
	remove_proc_entry("enable", speaker_proc_dir);
	remove_proc_entry("speaker", NULL);

	return;
}

static int __devinit speaker_probe(struct platform_device *pdev)
{
    printk("%s\n",__FUNCTION__);
	//ep = msm_rpc_connect(PM_LIBPROG, PM_LIBVERS, 0);
	return 0;
}



static int __devexit speaker_remove(struct platform_device *pdev)
{
    printk("%s\n",__FUNCTION__);
    //msm_rpc_close(ep);
	return 0;
}

static int speaker_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s\n",__FUNCTION__);
	cancel_delayed_work(&speaker_check_work);
	cancel_delayed_work(&speaker_check_work_off);

	return 0;
}

static int speaker_resume(struct platform_device *pdev)
{
	printk("%s\n",__FUNCTION__);

	return 0;
}


static struct platform_driver speaker_driver = {
	.probe		= speaker_probe,
	.remove		= __devexit_p(speaker_remove),
	.driver		= {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.suspend	= speaker_suspend,
	.resume	= speaker_resume
};

static struct platform_device speaker_device = {
	.name = DRIVER_NAME,
};

static int __init speaker_init(void)
{
	int retval = 0;
	printk("%s\n",__FUNCTION__);
	mutex_init(&speaker_lock);
	retval = init_proc();
	INIT_DELAYED_WORK(&speaker_check_work, speaker_check_func);
	INIT_DELAYED_WORK(&speaker_check_work_off, speaker_check_func_off);
	platform_device_register(&speaker_device);
	return platform_driver_register(&speaker_driver);
}
module_init(speaker_init);

static void __exit speaker_exit(void)
{
	printk("%s\n",__FUNCTION__);
	cleanup_proc();
	platform_driver_unregister(&speaker_driver);
	mutex_destroy(&speaker_lock);
}
module_exit(speaker_exit);

int speaker_audmgr_enable(bool on)
{
	int nResult = 0;
    audmgr_enabled = on ? 1 : 0;
	printk("%s audmgr_enabled=%d, speaker_enable=%d\n",__FUNCTION__,audmgr_enabled,(int)speaker_enable);
	if (on && speaker_enable == SPK_CMD_ON) 
	{
		nResult = speaker_cmd(SPK_CMD_DELAY_ON);
	}
	return nResult;
}

MODULE_DESCRIPTION("CCI speaker controller");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oscar Lee");
