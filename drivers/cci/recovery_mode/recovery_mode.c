/*
 * Copyright (C) 2010 CCI, Inc.
 * Author:	Oscar Lee
 * Date:	Feb.23, 2010
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
//#include <linux/platform_device.h>
#include <linux/io.h>
#include "../../../arch/arm/mach-msm/smd_private.h"
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/types.h>
#include <asm/uaccess.h>

/*******************************************************************************
*    Definitions
********************************************************************************/

/*******************************************************************************
*    Local Variable Declaration
********************************************************************************/
struct proc_dir_entry *recovery_entry;
//char recovery_mode;
/*******************************************************************************
*   Functions
********************************************************************************/

static int recovery_mode_read(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
    char *out = page;
    int len;
    cci_smem_value_t *smem_cci_smem_value;

    smem_cci_smem_value = smem_alloc( SMEM_CCI_SMEM_VALUE, sizeof( cci_smem_value_t ));
    out += sprintf(out, "%c\n", smem_cci_smem_value->os_version[0]);
    len = out - page - off;

    if (len < count) {
	*eof = 1;
        if (len <= 0) return 0;
    } else {
	len = count;
    }
    *start = page + off;

    return len;
}

int recovery_mode_write(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
    int len = 0;
    char *buf = "";
    cci_smem_value_t *smem_cci_smem_value;

    smem_cci_smem_value = smem_alloc( SMEM_CCI_SMEM_VALUE, sizeof( cci_smem_value_t ));

    if (count >= 2)
	len = 2;
    else
	len = count;

    buf = kmalloc(len,GFP_KERNEL);

    if (copy_from_user(buf, buffer,len)) {
	printk("error to get value\n");
	return -EFAULT;
    }

//    recovery_mode = buf[0];
    smem_cci_smem_value->os_version[0] = buf[0];

    kfree(buf);
    return len;
}


static int init_proc(void)
{
    recovery_entry = create_proc_entry("recovery_mode",S_IRUGO | S_IWUSR, NULL);

    if (recovery_entry){
	recovery_entry->read_proc = recovery_mode_read;
	recovery_entry->write_proc = recovery_mode_write;
	recovery_entry->data = NULL;
    }else
	return -1;

    return 1;
}

static int __init recovery_mode_init(void)
{
	int rc;
	rc = init_proc();
	return rc;
}
module_init(recovery_mode_init);

static void __exit recovery_mode_exit(void)
{
	remove_proc_entry("recovery_mode", NULL);
	printk("%s\n",__FUNCTION__);
}
module_exit(recovery_mode_exit);

MODULE_DESCRIPTION("CCI recovery mode setting");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oscar Lee");
