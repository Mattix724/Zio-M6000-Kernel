/* arch/arm/mach-msm/pm2.c
 *
 * MSM Power Management Routines
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/pm_qos_params.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <mach/msm_iomap.h>
#include <mach/system.h>
#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif
#ifdef CONFIG_VFP
#include <asm/vfp.h>
#endif
//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
#include <mach/msm_rpcrouter.h>
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI

#include "smd_private.h"
#include "smd_rpcrouter.h"
#include "acpuclock.h"
#include "clock.h"
#include "proc_comm.h"
#include "idle.h"
#include "irq.h"
#include "gpio.h"
#include "timer.h"
#include "pm.h"
#include "spm.h"

/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_PM_DEBUG_SUSPEND = 1U << 0,
	MSM_PM_DEBUG_POWER_COLLAPSE = 1U << 1,
	MSM_PM_DEBUG_STATE = 1U << 2,
	MSM_PM_DEBUG_CLOCK = 1U << 3,
	MSM_PM_DEBUG_RESET_VECTOR = 1U << 4,
	MSM_PM_DEBUG_SMSM_STATE = 1U << 5,
	MSM_PM_DEBUG_IDLE = 1U << 6,
};

static int msm_pm_debug_mask;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

#define MSM_PM_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & msm_pm_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)

#define MSM_PM_DEBUG_PRINT_STATE(tag) \
	do { \
		MSM_PM_DPRINTK(MSM_PM_DEBUG_STATE, \
			KERN_INFO, "%s: " \
			"APPS_CLK_SLEEP_EN %x, APPS_PWRDOWN %x, " \
			"SMSM_POWER_MASTER_DEM %x, SMSM_MODEM_STATE %x, " \
			"SMSM_APPS_DEM %x\n", \
			tag, \
			readl(APPS_CLK_SLEEP_EN), readl(APPS_PWRDOWN), \
			smsm_get_state(SMSM_POWER_MASTER_DEM), \
			smsm_get_state(SMSM_MODEM_STATE), \
			smsm_get_state(SMSM_APPS_DEM)); \
	} while (0)

#define MSM_PM_DEBUG_PRINT_SLEEP_INFO() \
	do { \
		if (msm_pm_debug_mask & MSM_PM_DEBUG_SMSM_STATE) \
			smsm_print_sleep_info(msm_pm_smem_data->sleep_time, \
				msm_pm_smem_data->resources_used, \
				msm_pm_smem_data->irq_mask, \
				msm_pm_smem_data->wakeup_reason, \
				msm_pm_smem_data->pending_irqs); \
	} while (0)


/******************************************************************************
 * Sleep Modes and Parameters
 *****************************************************************************/

static int msm_pm_sleep_mode = CONFIG_MSM7X00A_SLEEP_MODE;
module_param_named(
	sleep_mode, msm_pm_sleep_mode,
	int, S_IRUGO | S_IWUSR | S_IWGRP
);

static int msm_pm_idle_sleep_mode = CONFIG_MSM7X00A_IDLE_SLEEP_MODE;
module_param_named(
	idle_sleep_mode, msm_pm_idle_sleep_mode,
	int, S_IRUGO | S_IWUSR | S_IWGRP
);

static int msm_pm_idle_sleep_min_time = CONFIG_MSM7X00A_IDLE_SLEEP_MIN_TIME;
module_param_named(
	idle_sleep_min_time, msm_pm_idle_sleep_min_time,
	int, S_IRUGO | S_IWUSR | S_IWGRP
);

#define MSM_PM_MODE_ATTR_SUSPEND_ENABLED "suspend_enabled"
#define MSM_PM_MODE_ATTR_IDLE_ENABLED "idle_enabled"
#define MSM_PM_MODE_ATTR_LATENCY "latency"
#define MSM_PM_MODE_ATTR_RESIDENCY "residency"
#define MSM_PM_MODE_ATTR_NR (4)

static char *msm_pm_sleep_mode_labels[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND] = " ",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = "power_collapse",
	[MSM_PM_SLEEP_MODE_APPS_SLEEP] = "apps_sleep",
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] =
		"ramp_down_and_wfi",
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = "wfi",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] =
		"power_collapse_no_xo_shutdown",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		"standalone_power_collapse",
};

static struct msm_pm_platform_data *msm_pm_modes;

static struct kobject *msm_pm_mode_kobjs[MSM_PM_SLEEP_MODE_NR];
static struct attribute_group *msm_pm_mode_attr_group[MSM_PM_SLEEP_MODE_NR];
static struct attribute **msm_pm_mode_attrs[MSM_PM_SLEEP_MODE_NR];
static struct kobj_attribute *msm_pm_mode_kobj_attrs[MSM_PM_SLEEP_MODE_NR];

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
#define RESET_TCXO_TOTAL_SLEEP_TIME		10000000	// 10 ms
#define RESET_TCXO_SLEEP_TIME			10000000	// 10 ms
#define DEFAULT_TCXO_SLEEP_TIME			1000000000	// 1000 ms

extern struct wake_lock main_wake_lock;
extern void cci_idle_disable_clocks(void);
extern void cci_idle_enable_clocks(void);
extern int cci_check_enabled_clock(int);
extern int cci_check_all_enabled_clock(void);
struct timespec comp_time;
int64_t enter_tcxo_time_limit;
int64_t tcxo_sleep_time = DEFAULT_TCXO_SLEEP_TIME;
int64_t t3 = 0;
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
int64_t uart_receive_rx_time_limit;
int64_t tr_receive_rx_from_user = 0;
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

/*
 * Write out the attribute.
 */
static ssize_t msm_pm_mode_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_SUSPEND_ENABLED)) {
			u32 arg = msm_pm_modes[i].suspend_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_IDLE_ENABLED)) {
			u32 arg = msm_pm_modes[i].idle_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_LATENCY)) {
			kp.arg = &msm_pm_modes[i].latency;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_RESIDENCY)) {
			kp.arg = &msm_pm_modes[i].residency;
			ret = param_get_ulong(buf, &kp);
		}

		break;
	}

	if (ret > 0) {
		strcat(buf, "\n");
		ret++;
	}

	return ret;
}

/*
 * Read in the new attribute value.
 */
static ssize_t msm_pm_mode_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_SUSPEND_ENABLED)) {
			kp.arg = &msm_pm_modes[i].suspend_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_IDLE_ENABLED)) {
			kp.arg = &msm_pm_modes[i].idle_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_LATENCY)) {
			kp.arg = &msm_pm_modes[i].latency;
			ret = param_set_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			MSM_PM_MODE_ATTR_RESIDENCY)) {
			kp.arg = &msm_pm_modes[i].residency;
			ret = param_set_ulong(buf, &kp);
		}

		break;
	}

	return ret ? ret : count;
}

/*
 * Add sysfs entries for the sleep modes.
 */
static int __init msm_pm_mode_sysfs_add(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *modes_kobj = NULL;

	struct kobject *kobj;
	struct attribute_group *attr_group;
	struct attribute **attrs;
	struct kobj_attribute *kobj_attrs;

	int i, k;
	int ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		printk(KERN_ERR "%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto mode_sysfs_add_cleanup;
	}

	modes_kobj = kobject_create_and_add("modes", module_kobj);
	if (!modes_kobj) {
		printk(KERN_ERR "%s: cannot create modes kobject\n", __func__);
		ret = -ENOMEM;
		goto mode_sysfs_add_cleanup;
	}

	for (i = 0; i < ARRAY_SIZE(msm_pm_mode_kobjs); i++) {
		if (!msm_pm_modes[i].supported)
			continue;

		kobj = kobject_create_and_add(
				msm_pm_sleep_mode_labels[i], modes_kobj);
		attr_group = kzalloc(sizeof(*attr_group), GFP_KERNEL);
		attrs = kzalloc(sizeof(*attrs) * (MSM_PM_MODE_ATTR_NR + 1),
				GFP_KERNEL);
		kobj_attrs = kzalloc(sizeof(*kobj_attrs) * MSM_PM_MODE_ATTR_NR,
				GFP_KERNEL);

		if (!kobj || !attr_group || !attrs || !kobj_attrs) {
			printk(KERN_ERR
				"%s: cannot create kobject or attributes\n",
				__func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_abort;
		}

		kobj_attrs[0].attr.name = MSM_PM_MODE_ATTR_SUSPEND_ENABLED;
		kobj_attrs[1].attr.name = MSM_PM_MODE_ATTR_IDLE_ENABLED;
		kobj_attrs[2].attr.name = MSM_PM_MODE_ATTR_LATENCY;
		kobj_attrs[3].attr.name = MSM_PM_MODE_ATTR_RESIDENCY;

		for (k = 0; k < MSM_PM_MODE_ATTR_NR; k++) {
			kobj_attrs[k].attr.mode = 0644;
			kobj_attrs[k].show = msm_pm_mode_attr_show;
			kobj_attrs[k].store = msm_pm_mode_attr_store;

			attrs[k] = &kobj_attrs[k].attr;
		}
		attrs[MSM_PM_MODE_ATTR_NR] = NULL;

		attr_group->attrs = attrs;
		ret = sysfs_create_group(kobj, attr_group);
		if (ret) {
			printk(KERN_ERR
				"%s: cannot create kobject attribute group\n",
				__func__);
			goto mode_sysfs_add_abort;
		}

		msm_pm_mode_kobjs[i] = kobj;
		msm_pm_mode_attr_group[i] = attr_group;
		msm_pm_mode_attrs[i] = attrs;
		msm_pm_mode_kobj_attrs[i] = kobj_attrs;
	}

	return 0;

mode_sysfs_add_abort:
	kfree(kobj_attrs);
	kfree(attrs);
	kfree(attr_group);
	kobject_put(kobj);

mode_sysfs_add_cleanup:
	for (i = ARRAY_SIZE(msm_pm_mode_kobjs) - 1; i >= 0; i--) {
		if (!msm_pm_mode_kobjs[i])
			continue;

		sysfs_remove_group(
			msm_pm_mode_kobjs[i], msm_pm_mode_attr_group[i]);

		kfree(msm_pm_mode_kobj_attrs[i]);
		kfree(msm_pm_mode_attrs[i]);
		kfree(msm_pm_mode_attr_group[i]);
		kobject_put(msm_pm_mode_kobjs[i]);
	}

	return ret;
}

void __init msm_pm_set_platform_data(
	struct msm_pm_platform_data *data, int count)
{
	BUG_ON(MSM_PM_SLEEP_MODE_NR != count);
	msm_pm_modes = data;
}


/******************************************************************************
 * Sleep Limitations
 *****************************************************************************/
enum {
	SLEEP_LIMIT_NONE = 0,
	SLEEP_LIMIT_NO_TCXO_SHUTDOWN = 2,
	SLEEP_LIMIT_MASK = 0x03,
};

#ifdef CONFIG_MSM_MEMORY_LOW_POWER_MODE
enum {
	SLEEP_RESOURCE_MEMORY_BIT0 = 0x0200,
	SLEEP_RESOURCE_MEMORY_BIT1 = 0x0010,
};
#endif


/******************************************************************************
 * Configure Hardware for Power Down/Up
 *****************************************************************************/

#if defined(CONFIG_ARCH_MSM7X30)
#define APPS_CLK_SLEEP_EN (MSM_GCC_BASE + 0x020)
#define APPS_PWRDOWN      (MSM_ACC_BASE + 0x01c)
#define APPS_SECOP        (MSM_TCSR_BASE + 0x038)
#else /* defined(CONFIG_ARCH_MSM7X30) */
#define APPS_CLK_SLEEP_EN (MSM_CSR_BASE + 0x11c)
#define APPS_PWRDOWN      (MSM_CSR_BASE + 0x440)
#define APPS_STANDBY_CTL  (MSM_CSR_BASE + 0x108)
#endif /* defined(CONFIG_ARCH_MSM7X30) */

/*
 * Configure hardware registers in preparation for Apps power down.
 */
static void msm_pm_config_hw_before_power_down(void)
{
#if defined(CONFIG_ARCH_MSM7X30)
	writel(1, APPS_PWRDOWN);
	writel(4, APPS_SECOP);
#elif defined(CONFIG_ARCH_MSM7X27)
	writel(0x1f, APPS_CLK_SLEEP_EN);
	writel(1, APPS_PWRDOWN);
#else
	writel(0x1f, APPS_CLK_SLEEP_EN);
	writel(1, APPS_PWRDOWN);
	writel(0, APPS_STANDBY_CTL);
#endif
}

/*
 * Clear hardware registers after Apps powers up.
 */
static void msm_pm_config_hw_after_power_up(void)
{
#if defined(CONFIG_ARCH_MSM7X30)
	writel(0, APPS_SECOP);
	writel(0, APPS_PWRDOWN);
	msm_spm_reinit();
#else
	writel(0, APPS_PWRDOWN);
	writel(0, APPS_CLK_SLEEP_EN);
#endif
}

/*
 * Configure hardware registers in preparation for SWFI.
 */
static void msm_pm_config_hw_before_swfi(void)
{
#if defined(CONFIG_ARCH_QSD8X50)
	writel(0x1f, APPS_CLK_SLEEP_EN);
#elif defined(CONFIG_ARCH_MSM7X27)
	writel(0x0f, APPS_CLK_SLEEP_EN);
#endif
}

/*
 * Respond to timing out waiting for Modem
 *
 * NOTE: The function never returns.
 */
static void msm_pm_timeout(void)
{
#if defined(CONFIG_MSM_PM_TIMEOUT_RESET_CHIP)
	printk(KERN_EMERG "%s(): resetting chip\n", __func__);
	msm_proc_comm(PCOM_RESET_CHIP_IMM, NULL, NULL);
#elif defined(CONFIG_MSM_PM_TIMEOUT_RESET_MODEM)
	printk(KERN_EMERG "%s(): resetting modem\n", __func__);
	msm_proc_comm_reset_modem_now();
#elif defined(CONFIG_MSM_PM_TIMEOUT_HALT)
	printk(KERN_EMERG "%s(): halting\n", __func__);
#endif
	for (;;)
		;
}


/******************************************************************************
 * State Polling Definitions
 *****************************************************************************/

struct msm_pm_polled_group {
	uint32_t group_id;

	uint32_t bits_all_set;
	uint32_t bits_all_clear;
	uint32_t bits_any_set;
	uint32_t bits_any_clear;

	uint32_t value_read;
};

/*
 * Return true if all bits indicated by flag are set in source.
 */
static inline bool msm_pm_all_set(uint32_t source, uint32_t flag)
{
	return (source & flag) == flag;
}

/*
 * Return true if any bit indicated by flag are set in source.
 */
static inline bool msm_pm_any_set(uint32_t source, uint32_t flag)
{
	return !flag || (source & flag);
}

/*
 * Return true if all bits indicated by flag are cleared in source.
 */
static inline bool msm_pm_all_clear(uint32_t source, uint32_t flag)
{
	return (~source & flag) == flag;
}

/*
 * Return true if any bit indicated by flag are cleared in source.
 */
static inline bool msm_pm_any_clear(uint32_t source, uint32_t flag)
{
	return !flag || (~source & flag);
}

/*
 * Poll the shared memory states as indicated by the poll groups.
 *
 * nr_grps: number of groups in the array
 * grps: array of groups
 *
 * The function returns when conditions specified by any of the poll
 * groups become true.  The conditions specified by a poll group are
 * deemed true when 1) at least one bit from bits_any_set is set OR one
 * bit from bits_any_clear is cleared; and 2) all bits in bits_all_set
 * are set; and 3) all bits in bits_all_clear are cleared.
 *
 * Return value:
 *      >=0: index of the poll group whose conditions have become true
 *      -ETIMEDOUT: timed out
 */
static int msm_pm_poll_state(int nr_grps, struct msm_pm_polled_group *grps)
{
	int i, k;

	for (i = 0; i < 50000; i++) {
		for (k = 0; k < nr_grps; k++) {
			bool all_set, all_clear;
			bool any_set, any_clear;

			grps[k].value_read = smsm_get_state(grps[k].group_id);

			all_set = msm_pm_all_set(grps[k].value_read,
					grps[k].bits_all_set);
			all_clear = msm_pm_all_clear(grps[k].value_read,
					grps[k].bits_all_clear);
			any_set = msm_pm_any_set(grps[k].value_read,
					grps[k].bits_any_set);
			any_clear = msm_pm_any_clear(grps[k].value_read,
					grps[k].bits_any_clear);

			if (all_set && all_clear && (any_set || any_clear))
				return k;
		}
		udelay(50);
	}

	printk(KERN_ERR "%s failed:\n", __func__);
	for (k = 0; k < nr_grps; k++)
		printk(KERN_ERR "(%x, %x, %x, %x) %x\n",
			grps[k].bits_all_set, grps[k].bits_all_clear,
			grps[k].bits_any_set, grps[k].bits_any_clear,
			grps[k].value_read);

	return -ETIMEDOUT;
}


/******************************************************************************
 * Suspend Max Sleep Time
 *****************************************************************************/

#define SCLK_HZ (32768)
#define MSM_PM_SLEEP_TICK_LIMIT (0x6DDD000)

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);
#endif

static uint32_t msm_pm_max_sleep_time;

/*
 * Convert time from nanoseconds to slow clock ticks, then cap it to the
 * specified limit
 */
static int64_t msm_pm_convert_and_cap_time(int64_t time_ns, int64_t limit)
{
	do_div(time_ns, NSEC_PER_SEC / SCLK_HZ);
	return (time_ns > limit) ? limit : time_ns;
}

/*
 * Set the sleep time for suspend.  0 means infinite sleep time.
 */
void msm_pm_set_max_sleep_time(int64_t max_sleep_time_ns)
{
	unsigned long flags;

	local_irq_save(flags);
	if (max_sleep_time_ns == 0) {
		msm_pm_max_sleep_time = 0;
	} else {
		msm_pm_max_sleep_time = (uint32_t)msm_pm_convert_and_cap_time(
			max_sleep_time_ns, MSM_PM_SLEEP_TICK_LIMIT);

		if (msm_pm_max_sleep_time == 0)
			msm_pm_max_sleep_time = 1;
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): Requested %lld ns Giving %u sclk ticks\n", __func__,
		max_sleep_time_ns, msm_pm_max_sleep_time);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(msm_pm_set_max_sleep_time);


/******************************************************************************
 * CONFIG_MSM_IDLE_STATS
 *****************************************************************************/

#ifdef CONFIG_MSM_IDLE_STATS
enum msm_pm_time_stats_id {
	MSM_PM_STAT_REQUESTED_IDLE,
	MSM_PM_STAT_IDLE_SPIN,
	MSM_PM_STAT_IDLE_WFI,
	MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_SLEEP,
	MSM_PM_STAT_IDLE_FAILED_SLEEP,
	MSM_PM_STAT_IDLE_POWER_COLLAPSE,
	MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE,
	MSM_PM_STAT_SUSPEND,
	MSM_PM_STAT_FAILED_SUSPEND,
	MSM_PM_STAT_NOT_IDLE,
	MSM_PM_STAT_COUNT
};

static struct msm_pm_time_stats {
	const char *name;
	int64_t first_bucket_time;
	int bucket[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t min_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t max_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int count;
	int64_t total_time;
} msm_pm_stats[MSM_PM_STAT_COUNT] = {
	[MSM_PM_STAT_REQUESTED_IDLE].name = "idle-request",
	[MSM_PM_STAT_REQUESTED_IDLE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_SPIN].name = "idle-spin",
	[MSM_PM_STAT_IDLE_SPIN].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_WFI].name = "idle-wfi",
	[MSM_PM_STAT_IDLE_WFI].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE].name =
		"idle-standalone-power-collapse",
	[MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE].name =
		"idle-failed-standalone-power-collapse",
	[MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_SLEEP].name = "idle-sleep",
	[MSM_PM_STAT_IDLE_SLEEP].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_FAILED_SLEEP].name = "idle-failed-sleep",
	[MSM_PM_STAT_IDLE_FAILED_SLEEP].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_POWER_COLLAPSE].name = "idle-power-collapse",
	[MSM_PM_STAT_IDLE_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].name =
		"idle-failed-power-collapse",
	[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_SUSPEND].name = "suspend",
	[MSM_PM_STAT_SUSPEND].first_bucket_time =
		CONFIG_MSM_SUSPEND_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_FAILED_SUSPEND].name = "failed-suspend",
	[MSM_PM_STAT_FAILED_SUSPEND].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,

	[MSM_PM_STAT_NOT_IDLE].name = "not-idle",
	[MSM_PM_STAT_NOT_IDLE].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET,
};

static uint32_t msm_pm_sleep_limit = SLEEP_LIMIT_NONE;
static DECLARE_BITMAP(msm_pm_clocks_no_tcxo_shutdown, NR_CLKS);

/*
 * Add the given time data to the statistics collection.
 */
static void msm_pm_add_stat(enum msm_pm_time_stats_id id, int64_t t)
{
	int i;
	int64_t bt;

	msm_pm_stats[id].total_time += t;
	msm_pm_stats[id].count++;

	bt = t;
	do_div(bt, msm_pm_stats[id].first_bucket_time);

	if (bt < 1ULL << (CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT *
				(CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1)))
		i = DIV_ROUND_UP(fls((uint32_t)bt),
					CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT);
	else
		i = CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1;

	msm_pm_stats[id].bucket[i]++;

	if (t < msm_pm_stats[id].min_time[i] || !msm_pm_stats[id].max_time[i])
		msm_pm_stats[id].min_time[i] = t;
	if (t > msm_pm_stats[id].max_time[i])
		msm_pm_stats[id].max_time[i] = t;
}

/*
 * Helper function of snprintf where buf is auto-incremented, size is auto-
 * decremented, and there is no return value.
 *
 * NOTE: buf and size must be l-values (e.g. variables)
 */
#define SNPRINTF(buf, size, format, ...) \
	do { \
		if (size > 0) { \
			int ret; \
			ret = snprintf(buf, size, format, ## __VA_ARGS__); \
			if (ret > size) { \
				buf += size; \
				size = 0; \
			} else { \
				buf += ret; \
				size -= ret; \
			} \
		} \
	} while (0)

/*
 * Write out the power management statistics.
 */
static int msm_pm_read_proc
	(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int i;
	char *p = page;
	char clk_name[16];

	if (count < 1024) {
		*start = (char *) 0;
		*eof = 0;
		return 0;
	}

	if (!off) {
		SNPRINTF(p, count, "Clocks against last TCXO shutdown:\n");
		for_each_bit(i, msm_pm_clocks_no_tcxo_shutdown, NR_CLKS) {
			clk_name[0] = '\0';
			msm_clock_get_name(i, clk_name, sizeof(clk_name));
			SNPRINTF(p, count, "  %s (id=%d)\n", clk_name, i);
		}

		SNPRINTF(p, count, "Last power collapse voted ");
		if ((msm_pm_sleep_limit & SLEEP_LIMIT_MASK) ==
			SLEEP_LIMIT_NONE)
			SNPRINTF(p, count, "for TCXO shutdown\n\n");
		else
			SNPRINTF(p, count, "against TCXO shutdown\n\n");

		*start = (char *) 1;
		*eof = 0;
	} else if (--off < ARRAY_SIZE(msm_pm_stats)) {
		int64_t bucket_time;
		int64_t s;
		uint32_t ns;

		s = msm_pm_stats[off].total_time;
		ns = do_div(s, NSEC_PER_SEC);
		SNPRINTF(p, count,
			"%s:\n"
			"  count: %7d\n"
			"  total_time: %lld.%09u\n",
			msm_pm_stats[off].name,
			msm_pm_stats[off].count,
			s, ns);

		bucket_time = msm_pm_stats[off].first_bucket_time;
		for (i = 0; i < CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1; i++) {
			s = bucket_time;
			ns = do_div(s, NSEC_PER_SEC);
			SNPRINTF(p, count,
				"   <%6lld.%09u: %7d (%lld-%lld)\n",
				s, ns, msm_pm_stats[off].bucket[i],
				msm_pm_stats[off].min_time[i],
				msm_pm_stats[off].max_time[i]);

			bucket_time <<= CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT;
		}

		SNPRINTF(p, count, "  >=%6lld.%09u: %7d (%lld-%lld)\n",
			s, ns, msm_pm_stats[off].bucket[i],
			msm_pm_stats[off].min_time[i],
			msm_pm_stats[off].max_time[i]);

		*start = (char *) 1;
		*eof = (off + 1 >= ARRAY_SIZE(msm_pm_stats));
	}

	return p - page;
}
#undef SNPRINTF

#define MSM_PM_STATS_RESET "reset"

/*
 * Reset the power management statistics values.
 */
static int msm_pm_write_proc(struct file *file, const char __user *buffer,
	unsigned long count, void *data)
{
	char buf[sizeof(MSM_PM_STATS_RESET)];
	int ret;
	unsigned long flags;
	int i;

	if (count < strlen(MSM_PM_STATS_RESET)) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	if (copy_from_user(buf, buffer, strlen(MSM_PM_STATS_RESET))) {
		ret = -EFAULT;
		goto write_proc_failed;
	}

	if (memcmp(buf, MSM_PM_STATS_RESET, strlen(MSM_PM_STATS_RESET))) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	local_irq_save(flags);
	for (i = 0; i < ARRAY_SIZE(msm_pm_stats); i++) {
		memset(msm_pm_stats[i].bucket,
			0, sizeof(msm_pm_stats[i].bucket));
		memset(msm_pm_stats[i].min_time,
			0, sizeof(msm_pm_stats[i].min_time));
		memset(msm_pm_stats[i].max_time,
			0, sizeof(msm_pm_stats[i].max_time));
		msm_pm_stats[i].count = 0;
		msm_pm_stats[i].total_time = 0;
	}

	msm_pm_sleep_limit = SLEEP_LIMIT_NONE;
	bitmap_zero(msm_pm_clocks_no_tcxo_shutdown, NR_CLKS);
	local_irq_restore(flags);

	return count;

write_proc_failed:
	return ret;
}
#undef MSM_PM_STATS_RESET
#endif /* CONFIG_MSM_IDLE_STATS */


/******************************************************************************
 * Shared Memory Bits
 *****************************************************************************/

#define DEM_MASTER_BITS_PER_CPU             6

/* Power Master State Bits - Per CPU */
#define DEM_MASTER_SMSM_RUN \
	(0x01UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_RSA \
	(0x02UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_PWRC_EARLY_EXIT \
	(0x04UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_SLEEP_EXIT \
	(0x08UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_READY \
	(0x10UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_SLEEP \
	(0x20UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))

/* Power Slave State Bits */
#define DEM_SLAVE_SMSM_RUN                  (0x0001)
#define DEM_SLAVE_SMSM_PWRC                 (0x0002)
#define DEM_SLAVE_SMSM_PWRC_DELAY           (0x0004)
#define DEM_SLAVE_SMSM_PWRC_EARLY_EXIT      (0x0008)
#define DEM_SLAVE_SMSM_WFPI                 (0x0010)
#define DEM_SLAVE_SMSM_SLEEP                (0x0020)
#define DEM_SLAVE_SMSM_SLEEP_EXIT           (0x0040)
#define DEM_SLAVE_SMSM_MSGS_REDUCED         (0x0080)
#define DEM_SLAVE_SMSM_RESET                (0x0100)
#define DEM_SLAVE_SMSM_PWRC_SUSPEND         (0x0200)


/******************************************************************************
 * Shared Memory Data
 *****************************************************************************/

#define DEM_MAX_PORT_NAME_LEN (20)

struct msm_pm_smem_t {
	uint32_t sleep_time;
	uint32_t irq_mask;
	uint32_t resources_used;
	uint32_t reserved1;

	uint32_t wakeup_reason;
	uint32_t pending_irqs;
	uint32_t rpc_prog;
	uint32_t rpc_proc;
	char     smd_port_name[DEM_MAX_PORT_NAME_LEN];
	uint32_t reserved2;
};


/******************************************************************************
 *
 *****************************************************************************/
static struct msm_pm_smem_t *msm_pm_smem_data;
static uint32_t *msm_pm_reset_vector;
static atomic_t msm_pm_init_done = ATOMIC_INIT(0);

/*
 * Power collapse the Apps processor.  This function executes the handshake
 * protocol with Modem.
 *
 * Return value:
 *      -EAGAIN: modem reset occurred or early exit from power collapse
 *      -EBUSY: modem not ready for our power collapse -- no power loss
 *      -ETIMEDOUT: timed out waiting for modem's handshake -- no power loss
 *      0: success
 */
static int msm_pm_power_collapse
	(bool from_idle, uint32_t sleep_delay, uint32_t sleep_limit)
{
	struct msm_pm_polled_group state_grps[2];
	unsigned long saved_acpuclk_rate;
	uint32_t saved_vector[2];
	int collapsed = 0;
	int ret;

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO, "%s(): idle %d, delay %u, limit %u\n", __func__,
		(int)from_idle, sleep_delay, sleep_limit);

	if (!(smsm_get_state(SMSM_POWER_MASTER_DEM) & DEM_MASTER_SMSM_READY)) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO, "%s(): master not ready\n", __func__);
		ret = -EBUSY;
		goto power_collapse_bail;
	}

	memset(msm_pm_smem_data, 0, sizeof(*msm_pm_smem_data));

	msm_irq_enter_sleep1(true, from_idle, &msm_pm_smem_data->irq_mask);
	msm_gpio_enter_sleep(from_idle);

	msm_pm_smem_data->sleep_time = sleep_delay;
	msm_pm_smem_data->resources_used = sleep_limit;

	/* Enter PWRC/PWRC_SUSPEND */

	if (from_idle)
		smsm_change_state(SMSM_APPS_DEM, DEM_SLAVE_SMSM_RUN,
			DEM_SLAVE_SMSM_PWRC);
	else
		smsm_change_state(SMSM_APPS_DEM, DEM_SLAVE_SMSM_RUN,
			DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND);

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): PWRC");
	MSM_PM_DEBUG_PRINT_SLEEP_INFO();

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_RSA;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
		printk(KERN_EMERG "%s(): power collapse entry "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		goto power_collapse_early_exit;
	}

	/* DEM Master in RSA */

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): PWRC RSA");

	ret = msm_irq_enter_sleep2(true, from_idle);
	if (ret < 0) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_irq_enter_sleep2 aborted, %d\n", __func__,
			ret);
		goto power_collapse_early_exit;
	}

	msm_pm_config_hw_before_power_down();
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): pre power down");

	saved_acpuclk_rate = acpuclk_power_collapse();
	MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
		"%s(): change clock rate (old rate = %lu)\n", __func__,
		saved_acpuclk_rate);

	if (saved_acpuclk_rate == 0) {
		msm_pm_config_hw_after_power_up();
		goto power_collapse_early_exit;
	}

	saved_vector[0] = msm_pm_reset_vector[0];
	saved_vector[1] = msm_pm_reset_vector[1];
	msm_pm_reset_vector[0] = 0xE51FF004; /* ldr pc, 4 */
	msm_pm_reset_vector[1] = virt_to_phys(msm_pm_collapse_exit);

	MSM_PM_DPRINTK(MSM_PM_DEBUG_RESET_VECTOR, KERN_INFO,
		"%s(): vector %x %x -> %x %x\n", __func__,
		saved_vector[0], saved_vector[1],
		msm_pm_reset_vector[0], msm_pm_reset_vector[1]);

#ifdef CONFIG_VFP
	if (from_idle)
		vfp_flush_context();
#endif

#ifdef CONFIG_CACHE_L2X0
	l2x0_suspend();
#endif

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
	if(from_idle)
	{
		cci_idle_disable_clocks();
	}
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

	collapsed = msm_pm_collapse();

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
	if(from_idle)
	{
		cci_idle_enable_clocks();
	}
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

#ifdef CONFIG_CACHE_L2X0
	l2x0_resume(collapsed);
#endif

	msm_pm_reset_vector[0] = saved_vector[0];
	msm_pm_reset_vector[1] = saved_vector[1];

	if (collapsed) {
#ifdef CONFIG_VFP
		if (from_idle)
			vfp_reinit();
#endif
		cpu_init();
		local_fiq_enable();
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO,
		"%s(): msm_pm_collapse returned %d\n", __func__, collapsed);

	MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
		"%s(): restore clock rate to %lu\n", __func__,
		saved_acpuclk_rate);
	if (acpuclk_set_rate(smp_processor_id(), saved_acpuclk_rate,
			SETRATE_PC) < 0)
		printk(KERN_ERR "%s(): failed to restore clock rate(%lu)\n",
			__func__, saved_acpuclk_rate);

	msm_irq_exit_sleep1(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);

	msm_pm_config_hw_after_power_up();
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): post power up");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_any_set =
		DEM_MASTER_SMSM_RSA | DEM_MASTER_SMSM_PWRC_EARLY_EXIT;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
		printk(KERN_EMERG "%s(): power collapse exit "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		goto power_collapse_early_exit;
	}

	/* Sanity check */
	if (collapsed) {
		BUG_ON(!(state_grps[0].value_read & DEM_MASTER_SMSM_RSA));
	} else {
		BUG_ON(!(state_grps[0].value_read &
			DEM_MASTER_SMSM_PWRC_EARLY_EXIT));
		goto power_collapse_early_exit;
	}

	/* Enter WFPI */

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND,
		DEM_SLAVE_SMSM_WFPI);

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): WFPI");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_RUN;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
		printk(KERN_EMERG "%s(): power collapse WFPI "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		ret = -EAGAIN;
		goto power_collapse_restore_gpio_bail;
	}

	/* DEM Master == RUN */

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): WFPI RUN");
	MSM_PM_DEBUG_PRINT_SLEEP_INFO();

//[KB62] ===> BugID#645, #932 : Froyo porting : wakeup reason, added by Jimmy@CCI
#ifdef CONFIG_CCI_PM_LOG
	if(!from_idle)
	{
		printk("[SLEEP]%s():collapsed=%d, sleep_time=%u, sleep_limit=%u\n", __func__, collapsed, msm_pm_smem_data->sleep_time, msm_pm_smem_data->resources_used);
		switch(msm_pm_smem_data->wakeup_reason)
		{
			case SMSM_WKUP_REASON_RPC:
				printk("[WAKEUP]SMSM_WKUP_REASON_RPC, smd_port_name=%s, prog=0x%X, proc=0x%X\n", msm_pm_smem_data->smd_port_name, msm_pm_smem_data->rpc_prog, msm_pm_smem_data->rpc_proc);
				switch(msm_pm_smem_data->rpc_prog)
				{
					case 0x31000091://HS_REMCBPROG
						if(msm_pm_smem_data->rpc_proc == 1)//ONCRPC_HS_EVENT_CB_TYPE_PROC
						{
							printk("[WAKEUP]END/POWER key, HEADSET event\n");
						}
						else
						{
							printk("[WAKEUP]unknown handset event\n");
						}
						break;

					case 0x31000000://CMCBPROG
						if(msm_pm_smem_data->rpc_proc == 15)//ONCRPC_CM_MM_CALL_EVENT_F_TYPE_PROC
						{
							printk("[WAKEUP]Voice/Data call event\n");
						}
						break;

					case 0x31000003://WMSCBPROG
						if(msm_pm_smem_data->rpc_proc == 3)//ONCRPC_WMS_CLIENT_RELEASE_PROC
						{
							printk("[WAKEUP]SMS/WMS event\n");
						}
						break;

					default:
						break;
				}
				break;
			case SMSM_WKUP_REASON_INT://refer to irq-vic.c for INT#
				printk("[WAKEUP]SMSM_WKUP_REASON_INT, INT#=0x%X\n", msm_pm_smem_data->pending_irqs);
				break;
			case SMSM_WKUP_REASON_GPIO://refer to gpio_input.c for GPIO#
				printk("[WAKEUP]SMSM_WKUP_REASON_GPIO, GPIO#=%d\n", msm_pm_smem_data->reserved2);
				break;
			case SMSM_WKUP_REASON_TIMER:
				printk("[WAKEUP]SMSM_WKUP_REASON_TIMER\n");
				break;
			case SMSM_WKUP_REASON_ALARM:
				printk("[WAKEUP]SMSM_WKUP_REASON_ALARM\n");
				break;
			case SMSM_WKUP_REASON_RESET:
				printk("[WAKEUP]SMSM_WKUP_REASON_RESET\n");
				break;
			default:
				printk("[WAKEUP]unknown\n");
				printk("[WAKEUP]sleep_time=%d, irq_mask=0x%X, resources_used=0x%X, reserved1=0x%X\n", msm_pm_smem_data->sleep_time, msm_pm_smem_data->irq_mask, msm_pm_smem_data->resources_used, msm_pm_smem_data->reserved1);
				printk("[WAKEUP]wakeup_reason=0x%X, pending_irqs=0x%X, smd_port_name=%s, prog=0x%X, proc=0x%X, reserved2=0x%X\n", msm_pm_smem_data->wakeup_reason, msm_pm_smem_data->pending_irqs, msm_pm_smem_data->smd_port_name, msm_pm_smem_data->rpc_prog, msm_pm_smem_data->rpc_proc, msm_pm_smem_data->reserved2);
				break;
		}
	}
#endif // #ifdef CONFIG_CCI_PM_LOG
//[KB62] <=== BugID#645, #932 : Froyo porting : wakeup reason, added by Jimmy@CCI

	msm_irq_exit_sleep2(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);
	msm_irq_exit_sleep3(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);
	msm_gpio_exit_sleep();

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_WFPI, DEM_SLAVE_SMSM_RUN);

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): RUN");

	smd_sleep_exit();
	return 0;

power_collapse_early_exit:
	/* Enter PWRC_EARLY_EXIT */

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND,
		DEM_SLAVE_SMSM_PWRC_EARLY_EXIT);

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): EARLY_EXIT");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_PWRC_EARLY_EXIT;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): EARLY_EXIT EE");

	if (ret < 0) {
		printk(KERN_EMERG "%s(): power collapse EARLY_EXIT "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
	}

	/* DEM Master == RESET or PWRC_EARLY_EXIT */

	ret = -EAGAIN;

power_collapse_restore_gpio_bail:
	msm_gpio_exit_sleep();

	/* Enter RUN */
	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND |
		DEM_SLAVE_SMSM_PWRC_EARLY_EXIT, DEM_SLAVE_SMSM_RUN);

	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): RUN");

	if (collapsed)
		smd_sleep_exit();

power_collapse_bail:
	return ret;
}

/*
 * Power collapse the Apps processor without involving Modem.
 *
 * Return value:
 *      0: success
 */
static int msm_pm_power_collapse_standalone(void)
{
	uint32_t saved_vector[2];
	int collapsed = 0;
	int ret;

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO, "%s()\n", __func__);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_POWER_COLLAPSE, false);
	WARN_ON(ret);

	saved_vector[0] = msm_pm_reset_vector[0];
	saved_vector[1] = msm_pm_reset_vector[1];
	msm_pm_reset_vector[0] = 0xE51FF004; /* ldr pc, 4 */
	msm_pm_reset_vector[1] = virt_to_phys(msm_pm_collapse_exit);

	MSM_PM_DPRINTK(MSM_PM_DEBUG_RESET_VECTOR, KERN_INFO,
		"%s(): vector %x %x -> %x %x\n", __func__,
		saved_vector[0], saved_vector[1],
		msm_pm_reset_vector[0], msm_pm_reset_vector[1]);

#ifdef CONFIG_VFP
	vfp_flush_context();
#endif

#ifdef CONFIG_CACHE_L2X0
	l2x0_suspend();
#endif

	collapsed = msm_pm_collapse();

#ifdef CONFIG_CACHE_L2X0
	l2x0_resume(collapsed);
#endif

	msm_pm_reset_vector[0] = saved_vector[0];
	msm_pm_reset_vector[1] = saved_vector[1];

	if (collapsed) {
#ifdef CONFIG_VFP
		vfp_reinit();
#endif
		cpu_init();
		local_fiq_enable();
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO,
		"%s(): msm_pm_collapse returned %d\n", __func__, collapsed);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);

	return 0;
}

/*
 * Apps-sleep the Apps processor.  This function execute the handshake
 * protocol with Modem.
 *
 * Return value:
 *      -ENOSYS: function not implemented yet
 */
static int msm_pm_apps_sleep(uint32_t sleep_delay, uint32_t sleep_limit)
{
	return -ENOSYS;
}

/*
 * Bring the Apps processor to SWFI.
 *
 * Return value:
 *      -EIO: could not ramp Apps processor clock
 *      0: success
 */
static int msm_pm_swfi(bool ramp_acpu)
{
	unsigned long saved_acpuclk_rate = 0;

	if (ramp_acpu) {
		saved_acpuclk_rate = acpuclk_wait_for_irq();
		MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
			"%s(): change clock rate (old rate = %lu)\n", __func__,
			saved_acpuclk_rate);

		if (!saved_acpuclk_rate)
			return -EIO;
	}

	msm_pm_config_hw_before_swfi();
	msm_arch_idle();

	if (ramp_acpu) {
		MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
			"%s(): restore clock rate to %lu\n", __func__,
			saved_acpuclk_rate);
		if (acpuclk_set_rate(smp_processor_id(), saved_acpuclk_rate,
				SETRATE_SWFI) < 0)
			printk(KERN_ERR
				"%s(): failed to restore clock rate(%lu)\n",
				__func__, saved_acpuclk_rate);
	}

	return 0;
}

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
//do not enter txco shutdown if DUT receive character from uart3 rx
void cci_set_time_for_uart3_rx(void)
{
	tr_receive_rx_from_user = ktime_to_ns(ktime_get());
}
EXPORT_SYMBOL(cci_set_time_for_uart3_rx);
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

/******************************************************************************
 * External Idle/Suspend Functions
 *****************************************************************************/

/*
 * Put CPU in low power mode.
 */
void arch_idle(void)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;

	int latency_qos;
	int64_t timer_expiration;

	int low_power;
	int ret;
	int i;

#ifdef CONFIG_MSM_IDLE_STATS
	DECLARE_BITMAP(clk_ids, NR_CLKS);
	int64_t t1;
	static int64_t t2;
	int exit_stat;
#endif /* CONFIG_MSM_IDLE_STATS */
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
	int power_collapse_ret = -1;
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

	if (!atomic_read(&msm_pm_init_done))
		return;

	latency_qos = pm_qos_requirement(PM_QOS_CPU_DMA_LATENCY);
	timer_expiration = msm_timer_enter_idle();

#ifdef CONFIG_MSM_IDLE_STATS
	t1 = ktime_to_ns(ktime_get());
	msm_pm_add_stat(MSM_PM_STAT_NOT_IDLE, t1 - t2);
	msm_pm_add_stat(MSM_PM_STAT_REQUESTED_IDLE, timer_expiration);
#endif /* CONFIG_MSM_IDLE_STATS */

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		allow[i] = true;

	switch (msm_pm_idle_sleep_mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] =
			false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT:
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		allow[MSM_PM_SLEEP_MODE_APPS_SLEEP] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_APPS_SLEEP:
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = false;
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND:
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		break;
	default:
		printk(KERN_ERR "idle sleep mode is invalid: %d\n",
			msm_pm_idle_sleep_mode);
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = MSM_PM_STAT_IDLE_SPIN;
#endif /* CONFIG_MSM_IDLE_STATS */
		low_power = 0;
		goto arch_idle_exit;
	}

	if ((timer_expiration < msm_pm_idle_sleep_min_time) ||
#ifdef CONFIG_HAS_WAKELOCK
		has_wake_lock(WAKE_LOCK_IDLE) ||
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
		(has_wake_lock(WAKE_LOCK_SUSPEND) && !wake_lock_active(&main_wake_lock)) ||
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#endif
		!msm_irq_idle_sleep_allowed()) {
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = false;
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = false;
		allow[MSM_PM_SLEEP_MODE_APPS_SLEEP] = false;
	}

	for (i = 0; i < ARRAY_SIZE(allow); i++) {
		struct msm_pm_platform_data *mode = &msm_pm_modes[i];
		if (!mode->supported || !mode->idle_enabled ||
			mode->latency >= latency_qos ||
			mode->residency * 1000ULL >= timer_expiration)
			allow[i] = false;
	}

#ifdef CONFIG_MSM_IDLE_STATS
	ret = msm_clock_require_tcxo(clk_ids, NR_CLKS);
#elif defined(CONFIG_CLOCK_BASED_SLEEP_LIMIT)
	ret = msm_clock_require_tcxo(NULL, 0);
#endif /* CONFIG_MSM_IDLE_STATS */

#ifdef CONFIG_CLOCK_BASED_SLEEP_LIMIT
	if (ret)
		sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;
#endif

	MSM_PM_DPRINTK(MSM_PM_DEBUG_IDLE, KERN_INFO,
		"%s(): latency qos %d, next timer %lld, sleep limit %u\n",
		__func__, latency_qos, timer_expiration, sleep_limit);

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		MSM_PM_DPRINTK(MSM_PM_DEBUG_IDLE, KERN_INFO,
			"%s(): allow %s: %d\n", __func__,
			msm_pm_sleep_mode_labels[i], (int)allow[i]);

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] ||
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]) {
		uint32_t sleep_delay;

		sleep_delay = (uint32_t) msm_pm_convert_and_cap_time(
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
			tcxo_sleep_time, MSM_PM_SLEEP_TICK_LIMIT);
#else // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
			timer_expiration, MSM_PM_SLEEP_TICK_LIMIT);
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
		if (sleep_delay == 0) /* 0 would mean infinite time */
			sleep_delay = 1;

		if (!allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
			sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;

#if defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_IDLE_ACTIVE)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT1;
#elif defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_IDLE_RETENTION)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT0;
#endif

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
		if(		cci_check_all_enabled_clock()
#if defined(CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX) && defined(CONFIG_MSM_IDLE_STATS)
			|| (t1 - tr_receive_rx_from_user) < uart_receive_rx_time_limit
#endif // #if defined(CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX) && defined(CONFIG_MSM_IDLE_STATS)
		  )
		{
			ret = msm_pm_swfi(true);
			if (ret)
				while (!msm_irq_pending())
					udelay(1);

			if((msm_irq_pending() & (1U << 30)))
			{
				power_collapse_ret = 0;
			}

			low_power = 0;
#ifdef CONFIG_MSM_IDLE_STATS
			exit_stat = ret ? MSM_PM_STAT_IDLE_SPIN : MSM_PM_STAT_IDLE_WFI;
#endif // CONFIG_MSM_IDLE_STATS

			goto arch_idle_exit;
		}

		if((int64_t)t3 > 0)
		{
#ifdef CONFIG_MSM_IDLE_STATS
			if((t1 - t3) >= enter_tcxo_time_limit)
			{
				tcxo_sleep_time = DEFAULT_TCXO_SLEEP_TIME;
			}
			else
#endif // #ifdef CONFIG_MSM_IDLE_STATS
			{
				tcxo_sleep_time = timer_expiration;
			}
		}
		else
		{
			tcxo_sleep_time = DEFAULT_TCXO_SLEEP_TIME;
		}

		if(
				cci_check_enabled_clock(1)	//P_ADM_CLK
			||	cci_check_enabled_clock(9)	//P_I2C_CLK
			||	cci_check_enabled_clock(14)	//P_MDP_CLK
			||	cci_check_enabled_clock(17)	//P_PMDH_CLK
			||	cci_check_enabled_clock(19)	//P_SDC1_CLK
			||	cci_check_enabled_clock(20)	//P_SDC1_P_CLK
			||	cci_check_enabled_clock(33)	//P_UART3_CLK
			||	cci_check_enabled_clock(44)	//P_MDP_VSYNC_CLK

			||	cci_check_enabled_clock(8)	//P_GRP_3D_CLK
			||	cci_check_enabled_clock(12)	//P_IMEM_CLK
			||	cci_check_enabled_clock(51)	//P_GRP_3D_P_CLK
		  )
		{
			tcxo_sleep_time = timer_expiration;
		}

//printk("[kernel]arch_idle() : tcxo_sleep_time=%lld\n", tcxo_sleep_time);
		sleep_delay = (uint32_t) msm_pm_convert_and_cap_time(tcxo_sleep_time, MSM_PM_SLEEP_TICK_LIMIT);
		ret = msm_pm_power_collapse(true, sleep_delay, 0);
#else // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
		ret = msm_pm_power_collapse(true, sleep_delay, sleep_limit);
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] <=== BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
		low_power = (ret != -EBUSY && ret != -ETIMEDOUT);

#ifdef CONFIG_MSM_IDLE_STATS
		if (ret)
			exit_stat = MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE;
		else {
			exit_stat = MSM_PM_STAT_IDLE_POWER_COLLAPSE;
			msm_pm_sleep_limit = sleep_limit;
			bitmap_copy(msm_pm_clocks_no_tcxo_shutdown, clk_ids,
				NR_CLKS);
		}
#endif /* CONFIG_MSM_IDLE_STATS */
	} else if (allow[MSM_PM_SLEEP_MODE_APPS_SLEEP]) {
		uint32_t sleep_delay;

		sleep_delay = (uint32_t) msm_pm_convert_and_cap_time(
			timer_expiration, MSM_PM_SLEEP_TICK_LIMIT);
		if (sleep_delay == 0) /* 0 would mean infinite time */
			sleep_delay = 1;

		ret = msm_pm_apps_sleep(sleep_delay, sleep_limit);
		low_power = 0;

#ifdef CONFIG_MSM_IDLE_STATS
		if (ret)
			exit_stat = MSM_PM_STAT_IDLE_FAILED_SLEEP;
		else
			exit_stat = MSM_PM_STAT_IDLE_SLEEP;
#endif /* CONFIG_MSM_IDLE_STATS */
	} else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		ret = msm_pm_power_collapse_standalone();
		low_power = 0;
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = ret ?
			MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE :
			MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE;
#endif /* CONFIG_MSM_IDLE_STATS */
	} else if (allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT]) {
		ret = msm_pm_swfi(true);
		if (ret)
			while (!msm_irq_pending())
				udelay(1);
		low_power = 0;
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = ret ? MSM_PM_STAT_IDLE_SPIN : MSM_PM_STAT_IDLE_WFI;
#endif /* CONFIG_MSM_IDLE_STATS */
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		msm_pm_swfi(false);
		low_power = 0;
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = MSM_PM_STAT_IDLE_WFI;
#endif /* CONFIG_MSM_IDLE_STATS */
	} else {
		while (!msm_irq_pending())
			udelay(1);
		low_power = 0;
#ifdef CONFIG_MSM_IDLE_STATS
		exit_stat = MSM_PM_STAT_IDLE_SPIN;
#endif /* CONFIG_MSM_IDLE_STATS */
	}

arch_idle_exit:
	msm_timer_exit_idle(low_power);

#ifdef CONFIG_MSM_IDLE_STATS
	t2 = ktime_to_ns(ktime_get());
	msm_pm_add_stat(exit_stat, t2 - t1);
#endif /* CONFIG_MSM_IDLE_STATS */
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//record t1 when enter into arch_idle
//record t2 when prepare to leave arch_idle
//record t3 when deny to power collapse
	if(power_collapse_ret == 0)
	{
		t3 = ktime_to_ns(ktime_get());
	}
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
}

/*
 * Suspend the Apps processor.
 *
 * Return value:
 *      -EAGAIN: modem reset occurred or early exit from suspend
 *      -EBUSY: modem not ready for our suspend
 *      -EINVAL: invalid sleep mode
 *      -EIO: could not ramp Apps processor clock
 *      -ETIMEDOUT: timed out waiting for modem's handshake
 *      0: success
 */
static int msm_pm_enter(suspend_state_t state)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;
	int ret;
	int i;

#ifdef CONFIG_MSM_IDLE_STATS
	DECLARE_BITMAP(clk_ids, NR_CLKS);
	int64_t period = 0;
	int64_t time = 0;

	time = msm_timer_get_sclk_time(&period);
	ret = msm_clock_require_tcxo(clk_ids, NR_CLKS);
#elif defined(CONFIG_CLOCK_BASED_SLEEP_LIMIT)
	ret = msm_clock_require_tcxo(NULL, 0);
#endif /* CONFIG_MSM_IDLE_STATS */

#ifdef CONFIG_CLOCK_BASED_SLEEP_LIMIT
	if (ret)
		sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;
#endif

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): sleep limit %u\n", __func__, sleep_limit);

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		allow[i] = true;

	switch (msm_pm_sleep_mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] =
			false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT:
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		allow[MSM_PM_SLEEP_MODE_APPS_SLEEP] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_APPS_SLEEP:
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = false;
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = false;
		/* fall through */
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND:
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		break;
	default:
		printk(KERN_ERR "suspend sleep mode is invalid: %d\n",
			msm_pm_sleep_mode);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(allow); i++) {
		struct msm_pm_platform_data *mode = &msm_pm_modes[i];
		if (!mode->supported || !mode->suspend_enabled)
			allow[i] = false;
	}

	ret = 0;

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] ||
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]) {
#ifdef CONFIG_MSM_IDLE_STATS
		enum msm_pm_time_stats_id id;
		int64_t end_time;
#endif

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
		if (msm_pm_sleep_time_override > 0) {
			int64_t ns;
			ns = NSEC_PER_SEC * (int64_t)msm_pm_sleep_time_override;
			msm_pm_set_max_sleep_time(ns);
			msm_pm_sleep_time_override = 0;
		}
#endif
		if (!allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
			sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;

#if defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_ACTIVE)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT1;
#elif defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_RETENTION)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT0;
#endif

		ret = msm_pm_power_collapse(
			false, msm_pm_max_sleep_time, sleep_limit);

#ifdef CONFIG_MSM_IDLE_STATS
		if (ret)
			id = MSM_PM_STAT_FAILED_SUSPEND;
		else {
			id = MSM_PM_STAT_SUSPEND;
			msm_pm_sleep_limit = sleep_limit;
			bitmap_copy(msm_pm_clocks_no_tcxo_shutdown, clk_ids,
				NR_CLKS);
		}

		if (time != 0) {
			end_time = msm_timer_get_sclk_time(NULL);
			if (end_time != 0) {
				time = end_time - time;
				if (time < 0)
					time += period;
			} else
				time = 0;
		}

		msm_pm_add_stat(id, time);
#endif
	} else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		ret = msm_pm_power_collapse_standalone();
	} else if (allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT]) {
		ret = msm_pm_swfi(true);
		if (ret)
			while (!msm_irq_pending())
				udelay(1);
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		msm_pm_swfi(false);
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): return %d\n", __func__, ret);

	return ret;
}

static struct platform_suspend_ops msm_pm_ops = {
	.enter = msm_pm_enter,
	.valid = suspend_valid_only_mem,
};


/******************************************************************************
 * Restart Definitions
 *****************************************************************************/

//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//ahung@cci rpc reboot
#define CCIL1PROG			0x30001000
#define CCIL1VERS			0x00010001

#define ONCRPC_CCI_L1_REBOOT		25
#define ONCRPC_CCI_L1_DEBUG_INFO_PROC	20

#define CCI_L1_DEBUG_DLMODE_CMD		1001
#define CCI_L1_MODEM_FORCE_CRASH_ITEM	2003

enum
{
	RPC_REBOOT_TYPE_HARDWARE_RESET,
	RPC_REBOOT_TYPE_RECOVERY,
};

//[KB62] ===> force into ARM9 fatal error mode when encountered kernel panic
#ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
static int cci_l1_rpc_force_fatal_error(void)
{
	int rpc_id;
	u32 cmd = CCI_L1_MODEM_FORCE_CRASH_ITEM;
//	char tempString[320] = {0};//result buffer

	struct msm_rpc_endpoint *ep = NULL;
	struct cci_l1_rpc_req
	{
		struct rpc_request_hdr hdr;
		u32 args1;
		u32 args2;
		u32 args3;
	} req;

	ep = msm_rpc_connect(CCIL1PROG, CCIL1VERS, 0);
	if(IS_ERR(ep))
	{
		printk(KERN_ERR "%s : init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}

	req.args1 = cpu_to_be32(cmd);//debug_idx
	req.args2 = cpu_to_be32(1);//row
	req.args3 = cpu_to_be32(320);//buffer
	rpc_id = msm_rpc_call(ep, ONCRPC_CCI_L1_DEBUG_INFO_PROC,
				&req, sizeof(req),
				5 * HZ);

	if(rpc_id < 0)
	{
		printk(KERN_ERR "%s : ONCRPC_CCI_L1_DEBUG_INFO_PROC failed id=%d\n", __func__, rpc_id);
	}

	msm_rpc_close(ep);

	return rpc_id;
}
#endif // #ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
//[KB62] <=== force into ARM9 fatal error mode when encountered kernel panic

//KB60 modem download mode RPC
static int cci_l1_rpc_reboot_modem_download_mode(void)
{
	int rpc_id;
	u32 reboot_cmd = CCI_L1_DEBUG_DLMODE_CMD;
	u32 no_use_arg = 1;

	struct msm_rpc_endpoint *ep = NULL;
	struct cci_l1_rpc_reboot_req
	{
		struct rpc_request_hdr hdr;
		u32 args1;
		u32 args2;
		u32 args3;
	} req;
	struct cci_l1_rpc_reboot_rep
	{
		struct rpc_reply_hdr hdr;
		char* args;
	} rep;

	ep = msm_rpc_connect(CCIL1PROG, CCIL1VERS, 0);
	if (IS_ERR(ep))
	{
		printk(KERN_ERR "%s : init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}

	req.args1 = cpu_to_be32(reboot_cmd);
	req.args2 = cpu_to_be32(no_use_arg);
	req.args3 = cpu_to_be32(no_use_arg);
	rpc_id = msm_rpc_call_reply(ep, ONCRPC_CCI_L1_DEBUG_INFO_PROC,
				&req, sizeof(req), &rep, sizeof(rep),
				5 * HZ);

	if (rpc_id < 0)
	{
		printk(KERN_ERR "%s : ONCRPC_CCI_L1_DEBUG_INFO_PROC failed id=%d\n", __func__, rpc_id);
	}

	msm_rpc_close(ep);

	return rpc_id;
}

static int cci_l1_rpc_reboot(signed long reboot_type)
{
	int rpc_id;
	struct msm_rpc_endpoint *ep = NULL;
	struct cci_l1_rpc_reboot_args
	{
		unsigned long type;
	};
	struct cci_rpc_lig_sen_req
	{
		struct rpc_request_hdr hdr;
		struct cci_l1_rpc_reboot_args args;
	} req;

	ep = msm_rpc_connect(CCIL1PROG, CCIL1VERS, 0);
	if (IS_ERR(ep))
	{
		printk(KERN_ERR "%s : init rpc failed! rc = %ld\n", __func__, PTR_ERR(ep));
		return PTR_ERR(ep);
	}

	req.args.type = cpu_to_be32(reboot_type);

	rpc_id = msm_rpc_call(ep, ONCRPC_CCI_L1_REBOOT,
				&req, sizeof(req),
				5 * HZ);

	if (rpc_id < 0)
	{
		printk(KERN_ERR "%s : ONCRPC_CCI_L1_DEBUG_INFO_PROC failed id=%d\n", __func__, rpc_id);
	}

	msm_rpc_close(ep);

	return rpc_id;
}
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI

static uint32_t restart_reason = 0x776655AA;

//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
static void msm_pm_power_off(unsigned data1, unsigned data2)
#else // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
static void msm_pm_power_off(void)
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
{
	msm_rpcrouter_close();
//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
	msm_proc_comm(PCOM_POWER_DOWN, &data1, &data2);
#else // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
	msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
	for (;;)
		;
}

static void msm_pm_restart(char str, const char *cmd)
{
//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
	int err=0;
	printk("%s : restart_reason=0x%X, str=%c, cmd=%s\n", __FUNCTION__, restart_reason, str, cmd);

	if(restart_reason == 0x776655AA)
	{
		err = cci_l1_rpc_reboot(RPC_REBOOT_TYPE_HARDWARE_RESET);
	}
	else if(restart_reason == 0x77665502)
	{
		err = cci_l1_rpc_reboot(RPC_REBOOT_TYPE_RECOVERY);
	}
	else if(restart_reason == 0x77665500)
	{
		err = cci_l1_rpc_reboot_modem_download_mode();
	}
//[KB62] ===> force into ARM9 fatal error mode when encountered kernel panic
#ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
	else if(restart_reason == 0x776655FF)
	{
		err = cci_l1_rpc_force_fatal_error();
	}
#endif // #ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
//[KB62] <=== force into ARM9 fatal error mode when encountered kernel panic
	else
	{
		err = cci_l1_rpc_reboot(RPC_REBOOT_TYPE_HARDWARE_RESET);
	}
#else // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
	msm_rpcrouter_close();
	msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI

	for (;;)
		;
}

static int msm_reboot_call
	(struct notifier_block *this, unsigned long code, void *_cmd)
{
	if ((code == SYS_RESTART) && _cmd) {
		char *cmd = _cmd;
		if (!strcmp(cmd, "bootloader")) {
			restart_reason = 0x77665500;
		} else if (!strcmp(cmd, "recovery")) {
			restart_reason = 0x77665502;
		} else if (!strcmp(cmd, "eraseflash")) {
			restart_reason = 0x776655EF;
//[KB62] ===> BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
#ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] ===> force into ARM9 fatal error mode when encountered kernel panic
#ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
		} else if (!strcmp(cmd, "panic")) {
			restart_reason = 0x776655FF;
#endif // #ifdef CONFIG_CCI_PANIC_FORCE_ARM9_FATAL_ERROR
//[KB62] <=== force into ARM9 fatal error mode when encountered kernel panic
		} else if (!strcmp(cmd, "oemsbl")) {
			restart_reason = 0x77665500;
#endif // #ifdef CONFIG_CCI_POWER_OFF_REASON_SUPPORT
//[KB62] <=== BugID#645 : Froyo porting : power off reason support, added by Jimmy@CCI
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned code = simple_strtoul(cmd + 4, 0, 16) & 0xff;
			restart_reason = 0x6f656d00 | code;
		} else {
			restart_reason = 0x77665501;
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block msm_reboot_notifier = {
	.notifier_call = msm_reboot_call,
};


/******************************************************************************
 *
 *****************************************************************************/

/*
 * Initialize the power management subsystem.
 *
 * Return value:
 *      -ENODEV: initialization failed
 *      0: success
 */
static int __init msm_pm_init(void)
{
#ifdef CONFIG_MSM_IDLE_STATS
	struct proc_dir_entry *d_entry;
#endif
	int ret;

	pm_power_off = msm_pm_power_off;
	arm_pm_restart = msm_pm_restart;
	register_reboot_notifier(&msm_reboot_notifier);

	msm_pm_smem_data = smem_alloc(SMEM_APPS_DEM_SLAVE_DATA,
		sizeof(*msm_pm_smem_data));
	if (msm_pm_smem_data == NULL) {
		printk(KERN_ERR "%s: failed to get smsm_data\n", __func__);
		return -ENODEV;
	}

#ifdef CONFIG_ARCH_MSM_SCORPION
	/* The bootloader is responsible for initializing many of Scorpion's
	 * coprocessor registers for things like cache timing. The state of
	 * these coprocessor registers is lost on reset, so part of the
	 * bootloader must be re-executed. Do not overwrite the reset vector
	 * or bootloader area.
	 */
	msm_pm_reset_vector = (uint32_t *) PAGE_OFFSET;
#else
	msm_pm_reset_vector = ioremap(0, PAGE_SIZE);
	if (msm_pm_reset_vector == NULL) {
		printk(KERN_ERR "%s: failed to map reset vector\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_ARCH_MSM_SCORPION */

	ret = msm_timer_init_time_sync(msm_pm_timeout);
	if (ret)
		return ret;

	ret = smsm_change_intr_mask(SMSM_POWER_MASTER_DEM, 0xFFFFFFFF, 0);
	if (ret) {
		printk(KERN_ERR "%s: failed to clear interrupt mask, %d\n",
			__func__, ret);
		return ret;
	}

#ifdef CONFIG_MSM_MEMORY_LOW_POWER_MODE
	/* The wakeup_reason field is overloaded during initialization time
	   to signal Modem that Apps will control the low power modes of
	   the memory.
	 */
	msm_pm_smem_data->wakeup_reason = 1;
	smsm_change_state(SMSM_APPS_DEM, 0, DEM_SLAVE_SMSM_RUN);
#endif

	BUG_ON(msm_pm_modes == NULL);

	atomic_set(&msm_pm_init_done, 1);
	suspend_set_ops(&msm_pm_ops);

	msm_pm_mode_sysfs_add();
#ifdef CONFIG_MSM_IDLE_STATS
	d_entry = create_proc_entry("msm_pm_stats",
			S_IRUGO | S_IWUSR | S_IWGRP, NULL);
	if (d_entry) {
		d_entry->read_proc = msm_pm_read_proc;
		d_entry->write_proc = msm_pm_write_proc;
		d_entry->data = NULL;
	}
#endif

//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//initialize enter_txco_time_limit
	comp_time.tv_sec = 1;
	comp_time.tv_nsec = 0;
	enter_tcxo_time_limit = ktime_to_ns(timespec_to_ktime(comp_time));
#ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
//initialize uart3 rx delay time
	comp_time.tv_sec = 1;
	comp_time.tv_nsec = 0;
	uart_receive_rx_time_limit = ktime_to_ns(timespec_to_ktime(comp_time));
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE_SERIAL_RX
#endif // #ifdef CONFIG_CCI_TURN_OFF_TCXO_DURING_CPU_IDLE
//[KB62] ===> BugID#645 : Froyo porting : turn off TCXO during CPU idle, added by Jimmy@CCI

	return 0;
}

late_initcall(msm_pm_init);
