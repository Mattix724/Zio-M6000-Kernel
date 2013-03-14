/*
 * Copyright (C) 2010 CCI, Inc.
 * Author:	Johnny Lee
 * Date:	Mar. 12, 2010
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
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>

/*******************************************************************************
*    Definitions
********************************************************************************/
#define CCIPROG	0x30001000
#define CCIVERS	0x00010001
#define ONCRPC_CCI_REBOOT 25
#define ONCRPC_CCI_L1_DEBUG_INFO_PROC 20
		
#define CCI_LI_DEBUG_REBOOT_CMD 1001

/*******************************************************************************
*    Local Variable Declaration
********************************************************************************/
struct proc_dir_entry *euu_entry;
static struct mutex euu_lock;
/*******************************************************************************
*   Functions
********************************************************************************/
	
//KB60 modem download mode RPC
static int cci_rpc_reboot_modem_download_mode(void)
{
	int rpc_id;
	u32 reboot_cmd = CCI_LI_DEBUG_REBOOT_CMD;
	u32 no_use_arg = 1;

	struct msm_rpc_endpoint *ep = NULL;

	struct cci_rpc_reboot_req {
	    struct rpc_request_hdr hdr;
	    u32 args1;
	    u32 args2;
	    u32 args3;
	} req;
	struct cci_rpc_reboot_rep {
	    struct rpc_reply_hdr hdr;
	    char* args;
	} rep;

	ep = msm_rpc_connect(CCIPROG, CCIVERS, 0);
	if (IS_ERR(ep))
	{
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}

	req.args1 = cpu_to_be32(reboot_cmd);
	req.args2 = cpu_to_be32(no_use_arg);
	req.args3 = cpu_to_be32(no_use_arg);
	rpc_id = msm_rpc_call_reply(ep, ONCRPC_CCI_L1_DEBUG_INFO_PROC,
				&req, sizeof(req), &rep, sizeof(rep),
				5 * HZ);

	if (rpc_id < 0)
		printk(KERN_ERR "ONCRPC_CCI_L1_DEBUG_INFO_PROC failed id=%d\n",rpc_id);
	
	msm_rpc_close(ep);

	return rpc_id;
}

static int euu_read(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	return 0;
}

static int euu_write(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	int ret = 0 , mutex_ret = 0;
	int i;
	char bufstr[64];
	char *password = "go easy update";

	mutex_ret = mutex_trylock(&euu_lock);

	if(mutex_ret !=1) {
		return EBUSY;
	}
	printk ("%s\n", __func__);

	bufstr[0] = '\0';
	ret = copy_from_user(&bufstr, buffer, (sizeof(bufstr)-1));
	if (ret) {
		printk(KERN_ERR "%s:%s():%d --> Fail to copy_from_user()\n", __FILE__, __func__, __LINE__);
		return 0;
	}
	if (count < sizeof(bufstr))
		bufstr[count] = '\0';
	else
		bufstr[sizeof(bufstr)-1] = '\0';

	printk("%s: receive password %s count:%ld\n",__func__,bufstr, count);

	for(i = 0, ret = 1; i < count-1; i++) {
		//printk("%s:-----------%d: %x %x\n",__func__,i+1,password[i] , bufstr[i]);
		if(password[i] != bufstr[i]) {
			ret = 0;
		}
	}

	if (ret != 0) {
		ret = cci_rpc_reboot_modem_download_mode();
		printk(KERN_INFO "%s: reboot ret:%d\n", __func__, ret);
	} else {
		printk(KERN_INFO "%s: reboot ret:%d\n", __func__, ret);	
	}

	mutex_unlock(&euu_lock);

	return count;
}

static int init_proc(void)
{
	euu_entry = create_proc_entry("euu", 0666, NULL);

	if (euu_entry){
		euu_entry->read_proc = euu_read;
		euu_entry->write_proc = euu_write;
		euu_entry->data = NULL;
	} else
		return -1;

	return 1;
}

static int __init euu_init(void)
{
	int rc;
	mutex_init(&euu_lock);
	rc = init_proc();
	return rc;
}
module_init(euu_init);

static void __exit euu_exit(void)
{
	remove_proc_entry("euu", NULL);
	mutex_destroy(&euu_lock);
	printk("%s\n",__FUNCTION__);
}
module_exit(euu_exit);

MODULE_DESCRIPTION("CCI easy update");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johnny Lee");
