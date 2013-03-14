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

#include <mach/vreg.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>


/* /proc/lightsensor */
static struct proc_dir_entry *USB_proc_dir = NULL;
int usb_switch_mode(int enable);
int usb_report_mode(void);
/* Lock */
//static struct mutex lightsensor_lock;


/*******************************************************************************
 * * Local Function Declaration
 * *******************************************************************************/

/*******************************************************************************
 * * External Variable Declaration
 * *******************************************************************************/

/*******************************************************************************
 * * External Function Declaration
 * *******************************************************************************/

static int
proc_USB_data_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	int len = 0;
	len += sprintf(page + len, "%d", usb_report_mode());
	//printk(KERN_ERR "[%s]:%s():count=%d,page=%s, len=%d \n", __FILE__, __func__, (int)count,(char *)page, len);
	return len;
}
#define Switch_Off	0	//Turn off RNDIS
#define Switch_On	1	//Turn on RNDIS
#define Request_Mode	2	//Return USB mode

static int
proc_USB_data_write(struct file *file,
			const char __user * buffer,
			unsigned long count, void *data)
{
	int mode=0;
	//printk(KERN_ERR "[%s]:%s():count=%d,buffer=%s\n", __FILE__, __func__, (int)count,(char *)buffer);
	if('0'==*(buffer))
		mode=Switch_Off;
	else if('1'==*(buffer))
		mode=Switch_On;
	return usb_switch_mode(mode);
}

static int init_proc(void)
{

	//struct proc_dir_entry *lightsensor_proc_enable = NULL;
	struct proc_dir_entry *USB_proc_data = NULL;
	int retval = 0;
	
	/* Create /proc/USB */
	if( !(USB_proc_dir = proc_mkdir("USB", NULL)) ) {
		printk(KERN_ERR "[%s]:%s():%d --> Cannot create /proc/USB\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		USB_proc_dir->owner = THIS_MODULE;
	}

	/* create data */
	if( !(USB_proc_data = create_proc_entry("data", 0666, USB_proc_dir)) ) {
		printk(KERN_ERR "[%s]:%s():%d --> Cannot create /proc/USB/data\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	}
	else {
		USB_proc_data->owner = THIS_MODULE;
		USB_proc_data->read_proc = proc_USB_data_read;
		USB_proc_data->write_proc = proc_USB_data_write;
	}

end:
	return retval;	
}

static int __init usb_switch_init(void)
{
	int retval;

	printk(KERN_INFO"[%s]:%s: init.\n",__FILE__, __func__);

	//mutex_init(&lightsensor_lock);

	retval = init_proc();

	return 0;
}

static void cleanup_proc(void)
{
	remove_proc_entry("USB", NULL);

	return;
}


static void __exit usb_switch_exit(void)
{	
	cleanup_proc();

	return;
}

module_init(usb_switch_init);
module_exit(usb_switch_exit);

MODULE_LICENSE("USB v1");
MODULE_AUTHOR("Regulus_Yang");
