/* driver/cci/keypad_qxdm/keypad_qxdm.c
 *
 * Copyright (C) 2010 CCI, Inc.
 * Author:	Johnny Lee
 * Date:	2010.1.4
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
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

/*******************************************************************************
 * * Local Variable Declaration
 * *******************************************************************************/
static DEFINE_MUTEX(keypad_qxdm_lock);
static struct proc_dir_entry *keypad_qxdm_proc_dir = NULL;
static wait_queue_head_t keypad_qxdm_wait;
static int qxdm_keycode = 0;
static bool qxdm_event = true;

/*******************************************************************************
 * * External Functions
 * *******************************************************************************/
void keypad_qxdm_send_code(int keycode)
{
	qxdm_keycode = keycode;
	if (qxdm_event == false) {
		qxdm_event = true;
		wake_up_interruptible(&keypad_qxdm_wait);
	}
}
/*******************************************************************************
 * * Local Functions
 * *******************************************************************************/
static int
proc_keypad_qxdm_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	int outlen = 0;

	qxdm_event = false;
	qxdm_keycode = 0;
	wait_event_interruptible(keypad_qxdm_wait, qxdm_event);
	mutex_lock(&keypad_qxdm_lock);	
	outlen = sprintf(page,"%d",qxdm_keycode);
	mutex_unlock(&keypad_qxdm_lock);

	return count;
}

static int init_proc(void)
{
	struct proc_dir_entry *keypad_qxdm_proc_data = NULL;
	int retval = 0;

	/* Create /proc/keypad_qxdm */
	if( !(keypad_qxdm_proc_dir = proc_mkdir("keypad_qxdm", NULL)) ) {
		printk(KERN_ERR "%s --> Cannot create /proc/keypad_qxdm\n", __func__);
		retval = -1;
		goto end;
	}
	//else {
	//	keypad_qxdm_proc_dir->owner = THIS_MODULE;
	//}

	/* create code node */
	if( !(keypad_qxdm_proc_data = create_proc_entry("code", 0666, keypad_qxdm_proc_dir)) ) {
		printk(KERN_ERR "%s --> Cannot create /proc/keypad_qxdm/code\n", __func__);
		retval = -1;
		goto end;
	}
	else {
		keypad_qxdm_proc_data->read_proc = proc_keypad_qxdm_read;
		//keypad_qxdm_proc_data->owner = THIS_MODULE;
	}
	
end:
	return retval;	
}

static int __init keypad_qxdm_init(void)
{
	int retval;

	printk(KERN_INFO "%s: keypad_qxdm driver init.\n", __func__);
	init_waitqueue_head(&keypad_qxdm_wait);
	retval = init_proc();

	return 0;
}

static void cleanup_proc(void)
{
	remove_proc_entry("code", keypad_qxdm_proc_dir);
	remove_proc_entry("keypad_qxdm", NULL);
	return;
}


static void __exit keypad_qxdm_exit(void)
{	
	printk(KERN_INFO "%s: keypad_qxdm driver has been unloaded.\n", __func__);
	cleanup_proc();
	return;
}

module_init(keypad_qxdm_init);
module_exit(keypad_qxdm_exit);

MODULE_LICENSE("GPL v2");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Johnny_Lee");
