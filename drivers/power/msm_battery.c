/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include <asm/atomic.h>

#include <mach/msm_rpcrouter.h>
#include <mach/msm_battery.h>
#include "../../arch/arm/mach-msm/acpuclock.h"
#include <linux/rtc.h> //bincent add

#include "../../arch/arm/mach-msm/smd_private.h"
#include "../../arch/arm/mach-msm/keypad-surf-ffa.h"
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <mach/mpp.h>

#include <linux/fs.h>
#include <linux/file.h>

//[Bug 807] Add battery log on framework layer, weijiun_ling, 2010.10.13
#include <linux/miscdevice.h>
//[Bug 927] Add the mechanism of RPC error recovery, weijiun_ling, 2010.11.01
#include <linux/delay.h>

#define BATTERY_LOGCAT        1

#define BATTERY_RPC_PROG	0x30000089
#define BATTERY_RPC_VERS	0x00010001

#define BATTERY_RPC_CB_PROG	0x31000089
#define BATTERY_RPC_CB_VERS	0x00010001

#define CHG_RPC_PROG		0x3000001a
#define CHG_RPC_VERS		0x00010001

#define BATTERY_REGISTER_PROC                                  2
#define BATTERY_GET_CLIENT_INFO_PROC                   	3
#define BATTERY_MODIFY_CLIENT_PROC                     	4
#define BATTERY_DEREGISTER_CLIENT_PROC			5
#define BATTERY_SERVICE_TABLES_PROC                    	6
#define BATTERY_IS_SERVICING_TABLES_ENABLED_PROC       	7
#define BATTERY_ENABLE_TABLE_SERVICING_PROC            	8
#define BATTERY_DISABLE_TABLE_SERVICING_PROC           	9
#define BATTERY_READ_PROC                              	10
#define BATTERY_MIMIC_LEGACY_VBATT_READ_PROC           	11
#define BATTERY_ENABLE_DISABLE_FILTER_PROC 		14

#define VBATT_FILTER			2

#define BATTERY_CB_TYPE_PROC 		1
#define BATTERY_CB_ID_ALL_ACTIV       	1
#define BATTERY_CB_ID_LOW_VOL		2

#define BATTERY_LOW             3400
#define BATTERY_HIGH            4200

//RPC index
//bincent
#define CCIPROG	0x30001000
#define CCIVERS	0x00010001
//end
#define ONCRPC_CHG_USB_SELECT_IMAX                1
#define ONCRPC_CHG_IS_CHARGING_PROC 		      2
#define ONCRPC_CHG_IS_CHARGER_VALID_PROC 	3
#define ONCRPC_CHG_IS_BATTERY_VALID_PROC 	4
#define ONCRPC_CHG_UI_EVENT_READ_PROC 		5
#define ONCRPC_CHG_GET_GENERAL_STATUS_PROC 	12
#define ONCRPC_CHARGER_API_VERSIONS_PROC 	0xffffffff
#define ONCRPC_CHG_GET_TEMP_PROC                    30
#define ONCPRC_CCI_READ_TX_POWER_PROC           34
#define ONCRPC_CCI_GET_CHARGING_UAH                39
#define ONCRPC_CCI_GET_AMSS_CHG_STATE  		 40
//[Bug 919] To modify getting the initialize voltage at bootup, weijiun_ling, 2010.10.28
#define ONCRPC_CCI_GET_VBAT_BEFORE_OS_RUN    41
//End

#define RPC_TYPE_REQ     0
#define RPC_TYPE_REPLY   1
#define RPC_REQ_REPLY_COMMON_HEADER_SIZE   (3 * sizeof(uint32_t))


#define CHARGER_API_VERSION  			0x00010003
#define DEFAULT_CHARGER_API_VERSION		0x00010001


#define BATT_RPC_TIMEOUT    10000	/* 10 sec */

#define INVALID_BATT_HANDLE    -1


#define SUSPEND_EVENT		(1UL << 0)
#define RESUME_EVENT		(1UL << 1)
#define CLEANUP_EVENT		(1UL << 2)


#define DEBUG  1

#if DEBUG
#define DBG(x...) printk(KERN_INFO x)
#else
#define DBG(x...) do {} while (0)
#endif

enum
{
    BATTERY_REGISTRATION_SUCCESSFUL = 0,
    BATTERY_DEREGISTRATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
    BATTERY_MODIFICATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
    BATTERY_INTERROGATION_SUCCESSFUL = BATTERY_REGISTRATION_SUCCESSFUL,
    BATTERY_CLIENT_TABLE_FULL = 1,
    BATTERY_REG_PARAMS_WRONG = 2,
    BATTERY_DEREGISTRATION_FAILED = 4,
    BATTERY_MODIFICATION_FAILED = 8,
    BATTERY_INTERROGATION_FAILED = 16,
    /* Client's filter could not be set because perhaps it does not exist */
    BATTERY_SET_FILTER_FAILED         = 32,
    /* Client's could not be found for enabling or disabling the individual
     * client */
    BATTERY_ENABLE_DISABLE_INDIVIDUAL_CLIENT_FAILED  = 64,
    BATTERY_LAST_ERROR = 128,
};

enum
{
    BATTERY_VOLTAGE_UP = 0,
    BATTERY_VOLTAGE_DOWN,
    BATTERY_VOLTAGE_ABOVE_THIS_LEVEL,
    BATTERY_VOLTAGE_BELOW_THIS_LEVEL,
    BATTERY_VOLTAGE_LEVEL,
    BATTERY_ALL_ACTIVITY,
    VBATT_CHG_EVENTS,
    BATTERY_VOLTAGE_UNKNOWN,
};

#define CHG_UI_EVENT__FIELDTEST    16       //[Bug 795], weijiun_ling
enum
{
    CHG_UI_EVENT__IDLE,	/* Starting point, no charger.  */
    CHG_UI_EVENT__NO_POWER,	/* No/Weak Battery + Weak Charger. */
    CHG_UI_EVENT__VERY_LOW_POWER,	/* No/Weak Battery + Strong Charger. */
    CHG_UI_EVENT__LOW_POWER,	/* Low Battery + Strog Charger.  */
    CHG_UI_EVENT__NORMAL_POWER, /* Enough Power for most applications. */
    CHG_UI_EVENT__DONE,	/* Done charging, batt full.  */
    CHG_UI_EVENT__BAD_TEMP, /*TEMP is too hot/cold*/
    CHG_UI_EVENT__TIMEOUT,/*state timeout*/
    CHG_UI_EVENT__INVALID,
    CHG_UI_EVENT__MAX32 = 0x7fffffff
};

/*
 * This enum contains defintions of the charger hardware status
 */
enum chg_charger_status_type
{
    /* The charger is good      */
    CHARGER_STATUS_GOOD,
    /* The charger is bad       */
    CHARGER_STATUS_BAD,
    /* The charger is weak      */
    CHARGER_STATUS_WEAK,
    /* Invalid charger status.  */
    CHARGER_STATUS_INVALID
};

static char *charger_status[] =
{
    "good", "bad", "weak", "invalid"
};

/*
 *This enum contains defintions of the charger hardware type
 */
enum chg_charger_hardware_type
{
    /* The charger is removed                 */
    CHARGER_TYPE_NONE,
    /* The charger is a regular wall charger   */
    CHARGER_TYPE_WALL,
    /* The charger is a PC USB                 */
    CHARGER_TYPE_USB_PC,
    /* The charger is a wall USB charger       */
    CHARGER_TYPE_USB_WALL,
    /* The charger is a USB carkit             */
    CHARGER_TYPE_USB_CARKIT,
    /* Invalid charger hardware status.        */
    CHARGER_TYPE_INVALID
};

static char *charger_type[] =
{
    "No charger", "wall", "USB PC", "USB wall", "USB car kit",
    "invalid charger"
};

/*
 *  This enum contains defintions of the battery status
 */
enum chg_battery_status_type
{
    /* The battery is good        */
    BATTERY_STATUS_GOOD,
    /* The battery is cold/hot    */
    BATTERY_STATUS_BAD_TEMP,
    /* The battery is bad         */
    BATTERY_STATUS_BAD,
    /* Invalid battery status.    */
    BATTERY_STATUS_INVALID
};

static char *battery_status[] =
{
    "good ", "bad temperature", "bad", "invalid"
};


/*
 *This enum contains defintions of the battery voltage level
 */
enum chg_battery_level_type
{
    /* The battery voltage is dead/very low (less than 3.2V)        */
    BATTERY_LEVEL_DEAD,
    /* The battery voltage is weak/low (between 3.2V and 3.4V)      */
    BATTERY_LEVEL_WEAK,
    /* The battery voltage is good/normal(between 3.4V and 4.2V)  */
    BATTERY_LEVEL_GOOD,
    /* The battery voltage is up to full (close to 4.2V)            */
    BATTERY_LEVEL_FULL,
    /* Invalid battery voltage level.                               */
    BATTERY_LEVEL_INVALID
};

static char *battery_level[] =
{
    "dead", "weak", "good", "full", "invalid"
};


/* Generic type definition used to enable/disable charger functions */
enum
{
    CHG_CMD_DISABLE,
    CHG_CMD_ENABLE,
    CHG_CMD_INVALID,
    CHG_CMD_MAX32 = 0x7fffffff
};

enum CALL_STATE_PROC
{
    PHONE_IDLE,
    PHONE_RINGING,
    PHONE_OFFHOOK,
    ENABLE_BATT_DBG_INFO,    //for debug
    ENABLE_CHARGING_FUNC,
    DISABLE_CHARGING_FUNC,
};

enum CHARGING_STATE_FLAG
{
    CHG_NONE,
    CHG_CHARGING,
    CHG_COMPLETE,
    CHG_OT
};



struct batt_client_registration_req
{

    struct rpc_request_hdr hdr;

    /* The voltage at which callback (CB) should be called. */
    u32 desired_batt_voltage;

    /* The direction when the CB should be called. */
    u32 voltage_direction;

    /* The registered callback to be called when voltage and
     * direction specs are met. */
    u32 batt_cb_id;

    /* The call back data */
    u32 cb_data;
    u32 more_data;
    u32 batt_error;
};

struct batt_client_registration_rep
{
    struct rpc_reply_hdr hdr;
    u32 batt_clnt_handle;
};

struct rpc_reply_batt_chg
{
    struct rpc_reply_hdr hdr;
    u32 	more_data;

    u32	charger_status;
    u32	charger_type;
    u32	battery_status;
    u32	battery_level;
    u32    battery_voltage;
    u32	battery_temp;
};

static struct rpc_reply_batt_chg rep_batt_chg;

struct msm_battery_info
{
    u32 voltage_max_design;
    u32 voltage_min_design;
    u32 chg_api_version;
    u32 batt_technology;

    u32 avail_chg_sources;
    u32 current_chg_source;

    u32 batt_status;
    u32 batt_health;
    u32 voltage_now;
    u32 charger_valid;
    u32 batt_valid;
    s32 batt_capacity;//bincent

    s32 charger_status;
    s32 charger_type;
    s32 battery_status;
    s32 battery_level;
    s32 battery_voltage;
    s32 battery_temp;
    s32 batt_temp;

    u32(*calculate_capacity) (u32 voltage);

    s32 batt_handle;

    spinlock_t lock;

    struct power_supply *msm_psy_ac;
    struct power_supply *msm_psy_usb;
    struct power_supply *msm_psy_batt;

    struct msm_rpc_endpoint *batt_ep;
    struct msm_rpc_endpoint *chg_ep;
    struct msm_rpc_endpoint *cci_ep;

    struct workqueue_struct *msm_batt_wq;
    struct delayed_work dwork;

    struct task_struct *cb_thread;

    struct wake_lock wlock;//bincent

    wait_queue_head_t wait_q;

    struct early_suspend early_suspend;

    atomic_t handle_event;
    atomic_t event_handled;

    u32 type_of_event;
    uint32_t vbatt_rpc_req_xid;
};

//[Bug 1008], To modify getting the initialize voltage at bootup (Including AC & USB), weijiun_ling, 2010.11.12
static int battery_capacity_down_table_volt_25_000_new[] =
{
    3400, 3680, 3690, 3720, 3750, 3770, 3780, 3790, 3805, 3820, 3836,
    3860, 3900, 3925, 3950, 3980, 4010, 4045, 4085, 4125, 4200,
    3680, 3690, 3720, 3750, 3770, 3780, 3790, 3805, 3820, 3836, 3860,
    3900, 3925, 3950, 3980, 4010, 4045, 4085, 4125, 4160
};
//End

static int battery_capacity_down_table_volt_25_100_new[] =
{
    3400, 3654, 3666, 3698, 3727, 3746, 3757, 3767, 3782, 3797, 3813,
    3837, 3875, 3901, 3927, 3958, 3989, 4025, 4065, 4105, 4200,
    3705, 3713, 3742, 3773, 3794, 3803, 3813, 3828, 3843, 3859, 3883,
    3925, 3949, 3973, 4002, 4031, 4065, 4105, 4144, 4160
};

static int battery_capacity_down_table_volt_25_200_new[] =
{
    3400, 3629, 3642, 3676, 3704, 3723, 3733, 3744, 3758, 3773, 3790,
    3814, 3850, 3877, 3904, 3936, 3968, 4005, 4045, 4086, 4200,
    3730, 3737, 3764, 3796, 3817, 3827, 3836, 3852, 3867, 3882, 3906,
    3950, 3973, 3996, 4024, 4052, 4085, 4125, 4163, 4160
};

static int battery_capacity_down_table_volt_25_300_new[] =
{
    3400, 3604, 3619, 3654, 3681, 3699, 3710, 3721, 3735, 3750, 3768,
    3791, 3824, 3853, 3881, 3913, 3947, 3984, 4025, 4066, 4200,
    3755, 3760, 3786, 3819, 3841, 3850, 3859, 3875, 3890, 3904, 3929,
    3976, 3997, 4019, 4047, 4073, 4106, 4145, 4183, 4160
};
static int battery_capacity_down_table_volt_25_400_new[] =
{
    3400, 3579, 3595, 3632, 3658, 3675, 3687, 3697, 3711, 3726, 3745,
    3767, 3799, 3829, 3858, 3891, 3926, 3964, 4005, 4047, 4200,
    3781, 3784, 3808, 3842, 3865, 3873, 3883, 3899, 3914, 3927, 3953,
    4001, 4021, 4042, 4069, 4094, 4126, 4165, 4202, 4160
};
static int battery_capacity_down_table_volt_25_500_new[] =
{
    3400, 3553, 3572, 3610, 3635, 3652, 3663, 3674, 3688, 3703, 3722,
    3744, 3774, 3805, 3835, 3869, 3905, 3944, 3985, 4027, 4200,
    3806, 3807, 3830, 3865, 3888, 3897, 3906, 3922, 3937, 3950, 3976,
    4026, 4045, 4065, 4091, 4115, 4146, 4185, 4222, 4160
};
static int battery_capacity_down_table_volt_25_600_new[] =
{
    3400, 3528, 3548, 3588, 3612, 3628, 3640, 3651, 3664, 3680, 3699,
    3721, 3749, 3781, 3812, 3847, 3884, 3924, 3965, 4008, 4200,
    3831, 3831, 3852, 3888, 3912, 3920, 3929, 3946, 3960, 3973, 3999,
    4051, 4069, 4088, 4113, 4136, 4166, 4205, 4241, 4160
};

static int battery_capacity_down_table_volt_25_800_new[] =
{
    3400, 3485, 3508, 3550, 3573, 3588, 3600, 3612, 3624, 3640, 3660,
    3682, 3706, 3739, 3773, 3809, 3849, 3889, 3931, 3975,4200,
    3875, 3871, 3890, 3927, 3952, 3960, 3968, 3986, 4000, 4012, 4038,
    4094, 4111, 4127, 4151, 4171, 4201, 4239, 4274, 4160
};

//[BugID:588] To update the battery current table, weijiun_ling, 2010.07.30
static int battery_capacity_down_table_volt_25_150_new[] =
{
    3400, 3642, 3654, 3687, 3716, 3735, 3745, 3755, 3770, 3785, 3802,
    3825, 3862, 3889, 3916, 3947, 3979, 4015, 4055, 4095, 4200,
    3717, 3725, 3753, 3784, 3805, 3815, 3825, 3840, 3855, 3870, 3895,
    3938, 3961, 3984, 4013, 4041, 4075, 4115, 4154, 4160
};

static int battery_capacity_down_table_volt_25_250_new[] =
{
    3400, 3616, 3631, 3665, 3693, 3711, 3722, 3732, 3746, 3762, 3779, 
    3802, 3837, 3865, 3893, 3924, 3958, 3995, 4035, 4076, 4200,
    3743, 3749, 3775, 3807, 3829, 3838, 3848, 3864, 3878, 3893, 3918,
    3963, 3985, 4007, 4036, 4062, 4095, 4135, 4173, 4160
};

static int battery_capacity_down_table_volt_25_350_new[] =
{
    3400, 3591, 3607, 3643, 3670, 3687, 3698, 3709, 3723, 3738, 3756, 
    3779, 3812, 3841, 3870, 3902, 3937, 3974, 4015, 4057, 4200,
    3768, 3772, 3797, 3830, 3853, 3862, 3871, 3887, 3902, 3916, 3941, 
    3988, 4009, 4030, 4058, 4083, 4116, 4155, 4193, 4160
};

static int battery_capacity_down_table_volt_25_450_new[] =
{
    3400, 3566, 3584, 3621, 3647, 3664, 3675, 3686, 3699, 3715, 3733, 
    3756, 3787, 3817, 3847, 3880, 3916, 3954, 3995, 4037, 4200, 
    3793, 3796, 3819, 3853, 3876, 3885, 3894, 3911, 3925, 3939, 3964, 
    4013, 4033, 4053, 4080, 4104, 4136, 4175, 4212, 4160
};

static int battery_capacity_down_table_volt_25_550_new[] =
{
    3400, 3541, 3560, 3599, 3624, 3640, 3652, 3663, 3676, 3691, 3711, 
    3733, 3762, 3793, 3824, 3858, 3895, 3934, 3975, 4018, 4200, 
    3819, 3819, 3841, 3876, 3900, 3908, 3917, 3934, 3949, 3961, 3987, 
    4038, 4057, 4076, 4102, 4125, 4156, 4195, 4231, 4160
};

static int battery_capacity_down_table_volt_25_650_new[] =
{
    3400, 3515, 3536, 3577, 3601, 3616, 3629, 3640, 3652, 3668, 3688, 
    3710, 3736, 3769, 3801, 3836, 3874, 3914, 3955, 3998, 4200,
    3844, 3843, 3863, 3899, 3924, 3931, 3940, 3958, 3972, 3984, 4010, 
    4064, 4081, 4099, 4124, 4146, 4176, 4215, 4251, 4160
};

static int battery_capacity_down_table_volt_25_700_new[] =
{
    3400, 3503, 3525, 3566, 3589, 3604, 3617, 3628, 3641, 3656, 3676,
    3698, 3724, 3757, 3789, 3825, 3863, 3904, 3945, 3989, 4200,
    3856, 3855, 3874, 3911, 3936, 3943, 3952, 3969, 3984, 3996, 4022,
    4076, 4093, 4111, 4135, 4157, 4186, 4225, 4261, 4160
};

static int battery_capacity_up_table_volt_25_ac_800[] =
{
    3400, 3800, 3833, 3865, 3897, 3916, 3934, 3947, 3960, 
    3977, 3994, 4020, 4046, 4073, 4100
};//0~70% 7scale 4150
static int battery_capacity_up_table_volt_25_ac_400[] =
{
    4026, 4057, 4119, 4200,
};//70~100%	3scale

static int battery_capacity_up_table_volt_25_usb_new[] =
{
    //3400, 3672, 3719, 3766, 3780, 3796, 3818, 3867, 3904, 4100
    3400, 3731, 3771, 3813, 3827, 3841, 3867, 3917, 3957, 4100
};
static int battery_capacity_up_table_volt_25_usb_new1[] =
{
    4100, 4175, 4200
};


enum TableMapping
{
    down_table_volt_25_000,
    down_table_volt_25_100,
    down_table_volt_25_150,
    down_table_volt_25_200,
    down_table_volt_25_250,
    down_table_volt_25_300,
    down_table_volt_25_350,
    down_table_volt_25_400,
    down_table_volt_25_450,
    down_table_volt_25_500,
    down_table_volt_25_550,
    down_table_volt_25_600,
    down_table_volt_25_650,
    down_table_volt_25_700,
    down_table_volt_25_800,
    MAX_DISCHARGE_TABLE,
    up_table_volt_25_ac_800,                 //Table_ac_charger_cc_mode
    up_table_volt_25_ac_400,                 //Table_ac_charger_cv_mode
    up_table_volt_25_usb_new,              //Table_usb_charger_cc_mode
    up_table_volt_25_usb_new1,            //Table_usb_charger_cv_mode
    MAX_CHARGE_TABLE
};

static int *pTableIndex[] =
{
    battery_capacity_down_table_volt_25_000_new,
    battery_capacity_down_table_volt_25_100_new,
    battery_capacity_down_table_volt_25_150_new,
    battery_capacity_down_table_volt_25_200_new,
    battery_capacity_down_table_volt_25_250_new,
    battery_capacity_down_table_volt_25_300_new,
    battery_capacity_down_table_volt_25_350_new,
    battery_capacity_down_table_volt_25_400_new,
    battery_capacity_down_table_volt_25_450_new,
    battery_capacity_down_table_volt_25_500_new,
    battery_capacity_down_table_volt_25_550_new,
    battery_capacity_down_table_volt_25_600_new,
    battery_capacity_down_table_volt_25_650_new,
    battery_capacity_down_table_volt_25_700_new,
    battery_capacity_down_table_volt_25_800_new,
    NULL,
    battery_capacity_up_table_volt_25_ac_800,
    battery_capacity_up_table_volt_25_ac_400,
    battery_capacity_up_table_volt_25_usb_new,
    battery_capacity_up_table_volt_25_usb_new1
};




static unsigned int cci_batt_device_mask = 0;
static unsigned int cci_cal_current;
static unsigned int pre_cci_cal_current = 100;
static unsigned int cci_batt_device_current[] = {650, 550, 350, 200, 100, 0, 50, 100, 100, 50, 50};

//static unsigned int abnormal_current_enable = 0;
//static int abnormal_current_count = 0;
//static int abnormal_current_accu = 0;

/* ACPU clock frequency/current mapping table */
/*static int acpuclk_table[5][2] = {
	{ 122880, 10 },
	{ 245760, 30 },
	{ 320000, 50 },
	{ 480000, 90 },
	{ 600000, 130 }
};*/


//weijiun_ling
static atomic_t  g_ChgVld = ATOMIC_INIT(NO_CHG);  //Getting charging info from usb
static atomic_t  g_IsSuspend = ATOMIC_INIT(false);
static atomic_t  g_IsOnCall = ATOMIC_INIT(false);


static int *previous_capacity_down_table= NULL;
static int charger_state = 0;
static int charger_flag = CHG_NONE;
static int call_state = 0;
static int Ichg = 0;

//[TEST]
#ifdef BATT_FOR_TEST_OT
static int counter = 0;
#endif
//ENd


struct timespec pre_ts;
struct rtc_time pre_tm;
struct timespec pre_ts_plugout;
struct rtc_time pre_tm_plugout;

//static int Charge_Tempopercent =0;


//keep green led
//static int keep_green_enable = 0;
//static unsigned remain_green=0;



//static int previous_capacity = 0;
//static int compensate_capacity_count = 0;
//static int compensate_capacity_first_count = 0;
//static int previous_capacity[8] ={0,0,0,0,0,0,0,0};
//static int smooth_capacity = 0;


//weijiun_ling
s64 T_Capacity_uAH = -12345678;   //Theo_capacity (uA)
struct delayed_work g_dwork;
u8   g_Shotdown_Count = 1;
u16  g_Chg_UI_event = CHG_UI_EVENT__INVALID;
s32  g_Resume_Capacity = 0;
u8    g_Suspend_Chg_Source = NO_CHG;        //Get chg_source before suspend
u8   g_Is_Run_suspend = false;
int  RF_POWER = 0;

//[BugID:607] [BSP] Move to USB at dormancy estate find can't unlock
u8 is_update_v3 = false;

//[Bug 807] Add battery log on framework layer, weijiun_ling, 2010.10.13
#ifdef BATTERY_LOGCAT
char dbglog[256];
#endif

static struct msm_battery_info msm_batt_info =
{
    .batt_handle = INVALID_BATT_HANDLE,
    .charger_status = -1,
    .charger_type = -1,
    .battery_status = -1,
    .battery_level = -1,
    .battery_voltage = -1,
    .battery_temp = -1,
};

static enum power_supply_property msm_power_props[] =
{
    POWER_SUPPLY_PROP_ONLINE,
};

static char *msm_power_supplied_to[] =
{
    "battery",
};

//bincent
static void msm_batt_update_psy_status_v0(void);
static void msm_batt_update_psy_status_v1(void);
static void msm_batt_update_psy_status_v2(void);
static void msm_batt_update_psy_status_v3(void);


static u32 msm_batt_capacity(u32 current_voltage);
static void msm_batt_wait_for_batt_chg_event(struct work_struct *work);
static void msm_batt_early_suspend(struct early_suspend *h);
static int msm_batt_cleanup(void);
static DECLARE_WORK(msm_batt_cb_work, msm_batt_wait_for_batt_chg_event);



void batt_cci_set_wall_charger_transistor_imaxsel(u8 val)
{
    int rc;

    struct rpc_batt_chg_req
    {
        struct rpc_request_hdr hdr;
        u32 data;
    } req;


    req.data = cpu_to_be32(val);
    rc = msm_rpc_call(msm_batt_info.cci_ep, ONCRPC_CHG_USB_SELECT_IMAX, &req, sizeof(req), 5 * HZ);
    printk("@batt_cci_set_wall_charger_transistor_imaxsel, rc:%d, val:%d\n",rc, val);

}

//for ##8378#  (for test mode)
void Check_FieldTest(u32 *chg_batt_event)
{
    if (chg_batt_event == NULL)
        return;

    if (call_state == DISABLE_CHARGING_FUNC)
    {
        if (msm_batt_info.current_chg_source != AC_CHG)  //Need to change type to NO_CHG
        {
            if (*chg_batt_event == 0xFF)    //value 0xFF is from plugging in/out charger
            {
                batt_cci_set_wall_charger_transistor_imaxsel(1);
                batt_cci_set_wall_charger_transistor_imaxsel(0);
            }
            g_Chg_UI_event = CHG_UI_EVENT__NORMAL_POWER;
            *chg_batt_event = CHG_UI_EVENT__FIELDTEST;
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            msm_batt_info.current_chg_source = NO_CHG;
        }
    }
    else if (call_state == ENABLE_CHARGING_FUNC)
    {
        g_Chg_UI_event = CHG_UI_EVENT__FIELDTEST;
        *chg_batt_event = CHG_UI_EVENT__NORMAL_POWER;
        batt_cci_set_wall_charger_transistor_imaxsel(1);
        msm_batt_info.current_chg_source = atomic_read(&g_ChgVld);
        call_state = PHONE_IDLE;
    }
}

void Update_Power_Status(void)
{
    if (msm_batt_info.msm_psy_batt && msm_batt_info.msm_psy_ac && msm_batt_info.msm_psy_usb )
    {
        power_supply_changed(msm_batt_info.msm_psy_batt);
        power_supply_changed(msm_batt_info.msm_psy_ac);
        power_supply_changed(msm_batt_info.msm_psy_usb);
    }
}

void UI_To_UpperLayer(u32 chg_batt_event)
{
    switch (chg_batt_event)
    {
        case CHG_UI_EVENT__VERY_LOW_POWER:
        case CHG_UI_EVENT__LOW_POWER:
        case CHG_UI_EVENT__NORMAL_POWER:
        {
            int type = atomic_read(&g_ChgVld);
            if (type > NO_CHG)
            {
                if (msm_batt_info.batt_capacity < 100)
                {
                    charger_flag = CHG_CHARGING;
                    msm_batt_info.batt_status = POWER_SUPPLY_STATUS_CHARGING;
                    msm_batt_info.current_chg_source = type;
                }
                else if (msm_batt_info.batt_capacity == 100)
                {
                    charger_flag = CHG_COMPLETE;
                    msm_batt_info.batt_status = POWER_SUPPLY_STATUS_FULL;
                    msm_batt_info.current_chg_source = type;    //[Bug 795]
                }
            }
            else
            {
                charger_flag = CHG_NONE;
                msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
                msm_batt_info.current_chg_source = NO_CHG;
            }
        }
        break;


        case CHG_UI_EVENT__DONE:
        {
            int type = atomic_read(&g_ChgVld);
            charger_flag = CHG_COMPLETE;
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_FULL;
            msm_batt_info.current_chg_source = type;    //[Bug 795]
        }

            break;

        case CHG_UI_EVENT__BAD_TEMP:
            charger_flag = CHG_OT;
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            msm_batt_info.current_chg_source = NO_CHG;
            break;

        case CHG_UI_EVENT__IDLE:
            //[Bug 795], Do not use CHG_UI_EVENT__IDLE state reported from Modem, weijiun_ling
            //Do nothing
            break;

        case CHG_UI_EVENT__TIMEOUT:
        case CHG_UI_EVENT__FIELDTEST:
        case CHG_UI_EVENT__NO_POWER:
        default:
            charger_flag = CHG_NONE;
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
            msm_batt_info.current_chg_source = NO_CHG;
            break;

    }

    //if (prev_batt_status != msm_batt_info.batt_status)
    if (g_Chg_UI_event != chg_batt_event)
    {
        printk("[Battery]: Change batt event %d ==> %d, batt_status:%d\n",
               g_Chg_UI_event, chg_batt_event, msm_batt_info.batt_status);
        power_supply_changed(msm_batt_info.msm_psy_ac);
        power_supply_changed(msm_batt_info.msm_psy_usb);

        g_Chg_UI_event = chg_batt_event;
    }

}


void Estimate_Current(s32 power_level_dbm)
{
    int i;

    //RF power level
    cci_batt_device_mask &= ~(0x1F);
    RF_POWER = 0;

    if (call_state == PHONE_OFFHOOK)
    {
        //[BugID:588] To update the battery current table, weijiun_ling, 2010.07.30
        if (power_level_dbm >= 230)
        {
            RF_POWER = 550;
        }
        else if (power_level_dbm >= 200)
        {
            RF_POWER = 500;
        }
        else if (power_level_dbm >= 180)
        {
            RF_POWER = 400;
        }
        else if (power_level_dbm >= 150)
        {
            RF_POWER = 350;
        }
        else if (power_level_dbm >= 90)
        {
            RF_POWER = 300;
        }
        else if (power_level_dbm >= 70)
        {
            RF_POWER = 250;
        }
        else if (power_level_dbm >= -20)
        {
            RF_POWER = 200;
        }
        else 
        {
            RF_POWER = 150;
        }

    }
    cci_cal_current = 150; //reset to base
    for (i = 0; i < ARRAY_SIZE(cci_batt_device_current); i++)
    {
        if (cci_batt_device_mask & (1 << i))
        {
            cci_cal_current += cci_batt_device_current[i];
        }
    }
    cci_cal_current += RF_POWER;

    pre_cci_cal_current = cci_cal_current;

}


//export to other driver
void cci_batt_device_status_update(int device, int status)
{
    /* If device on */
    //printk("device = %d , status = %d\n",device,status);
    if (status == 1)
        cci_batt_device_mask |= device;
    else
        cci_batt_device_mask &= ~device;

    printk(KERN_INFO "[Battery]: %s(), device:0x%x, cci_batt_device_mask:0x%x \n",
           __func__, device, cci_batt_device_mask);

    //printk("cci_batt_device_mask = 0x%x\n",cci_batt_device_mask);
}


void kb60_android_charger_usb_change(int plugin, int type)
{
    u32 chg_batt_event = 0xFF;
    printk("[Battery]: %s, plugin:%d, type:%d \n",__func__,plugin, type);

    if (plugin == 1)
    {
        atomic_set(&g_ChgVld, type);

#ifdef BATT_FOR_TEST_OT
//[TEST]
        counter = 1;
//[TEST]
#endif

        if (msm_batt_info.batt_capacity < 100)
        {
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_CHARGING;
        }
        else if (msm_batt_info.batt_capacity == 100)
        {
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_FULL;
        }

        //charger type 1=AC, 2=USB
        msm_batt_info.current_chg_source = type;
    }
    else
    {
        atomic_set(&g_ChgVld, NO_CHG);

        charger_flag = CHG_NONE;    //[Bug 795]
        msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        msm_batt_info.current_chg_source = NO_CHG;
    }

    Check_FieldTest(&chg_batt_event);

//B:[BugID:607] [BSP] Move to USB at dormancy estate find can't unlock
    cancel_delayed_work_sync(&msm_batt_info.dwork);
    if (atomic_read(&g_IsSuspend) == false)
    {
        schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(10*1000));
        Update_Power_Status();    //Delay updating power status at late resume
    }
    else
    {
        msm_batt_update_psy_status_v3();  //Need to run after batt_resume()
 
        if (type == AC_CHG)
            power_supply_changed(msm_batt_info.msm_psy_ac);
        else if (type == USB_CHG)
            power_supply_changed(msm_batt_info.msm_psy_usb);

        is_update_v3 = true;
        schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(30*1000));
    }
//E:[BugID:607] [BSP] Move to USB at dormancy estate find can't unlock

}


static int msm_power_get_property(struct power_supply *psy,
                                  enum power_supply_property psp,
                                  union power_supply_propval *val)
{
//printk(KERN_INFO "[Battery]: %s(), psp:%d\n", __func__,psp);

    switch (psp)
    {
        case POWER_SUPPLY_PROP_ONLINE:

            if (psy->type == POWER_SUPPLY_TYPE_MAINS)
            {

                val->intval = msm_batt_info.current_chg_source & AC_CHG
                              ? 1 : 0;
                //printk(KERN_INFO "%s(): power supply = %s online = %d\n"
                //, __func__, psy->name, val->intval);

            }

            if (psy->type == POWER_SUPPLY_TYPE_USB)
            {

                val->intval = msm_batt_info.current_chg_source & USB_CHG
                              ? 1 : 0;

                //printk(KERN_INFO "%s(): power supply = %s online = %d\n"
                //, __func__, psy->name, val->intval);
            }

            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct power_supply msm_psy_ac =
{
    .name = "ac",
    .type = POWER_SUPPLY_TYPE_MAINS,
    .supplied_to = msm_power_supplied_to,
    .num_supplicants = ARRAY_SIZE(msm_power_supplied_to),
    .properties = msm_power_props,
    .num_properties = ARRAY_SIZE(msm_power_props),
    .get_property = msm_power_get_property,
};

static struct power_supply msm_psy_usb =
{
    .name = "usb",
    .type = POWER_SUPPLY_TYPE_USB,
    .supplied_to = msm_power_supplied_to,
    .num_supplicants = ARRAY_SIZE(msm_power_supplied_to),
    .properties = msm_power_props,
    .num_properties = ARRAY_SIZE(msm_power_props),
    .get_property = msm_power_get_property,
};

static enum power_supply_property msm_batt_power_props[] =
{
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    //POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
    //POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_TEMP,
};


static void msm_batt_external_power_changed(struct power_supply *psy)
{
    printk(KERN_INFO "%s() : external power supply changed for %s\n",
           __func__, psy->name);
    power_supply_changed(psy);
}

static int msm_batt_power_get_property(struct power_supply *psy,
                                       enum power_supply_property psp,
                                       union power_supply_propval *val)
{

//printk(KERN_INFO "[Battery]: %s(), psp:%d\n", __func__,psp);

    switch (psp)
    {
        case POWER_SUPPLY_PROP_STATUS:
            val->intval = msm_batt_info.batt_status;
            break;
        case POWER_SUPPLY_PROP_HEALTH:
            val->intval = msm_batt_info.batt_health;
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            val->intval = msm_batt_info.batt_valid;
            break;
        case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = msm_batt_info.batt_technology;
            break;
            /*case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
            	val->intval = msm_batt_info.voltage_max_design;
            	break;
            case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
            	val->intval = msm_batt_info.voltage_min_design;
            	break;*/
        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = msm_batt_info.voltage_now;
            break;
        case POWER_SUPPLY_PROP_CAPACITY:
            val->intval = msm_batt_info.batt_capacity;
            break;
        case POWER_SUPPLY_PROP_TEMP:
            val->intval = msm_batt_info.batt_temp;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct power_supply msm_psy_batt =
{
    .name = "battery",
    .type = POWER_SUPPLY_TYPE_BATTERY,
    .properties = msm_batt_power_props,
    .num_properties = ARRAY_SIZE(msm_batt_power_props),
    .get_property = msm_batt_power_get_property,
    .external_power_changed = msm_batt_external_power_changed,
};

//weijiun_ling, no used
static int msm_batt_get_batt_chg_status_v1(void)
{
    int rc ;
    struct rpc_req_batt_chg
    {
        struct rpc_request_hdr hdr;
        u32 more_data;
    } req_batt_chg;

    req_batt_chg.more_data = cpu_to_be32(1);

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    memset(&rep_batt_chg, 0, sizeof(rep_batt_chg));

    rc = msm_rpc_call_reply(msm_batt_info.chg_ep,
                            ONCRPC_CHG_GET_GENERAL_STATUS_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_batt_chg, sizeof(rep_batt_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR
               "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_GET_GENERAL_STATUS_PROC, rc);
        return rc;
    }
    else if (be32_to_cpu(rep_batt_chg.more_data))
    {

        rep_batt_chg.charger_status =
            be32_to_cpu(rep_batt_chg.charger_status);

        rep_batt_chg.charger_type =
            be32_to_cpu(rep_batt_chg.charger_type);

        rep_batt_chg.battery_status =
            be32_to_cpu(rep_batt_chg.battery_status);

        rep_batt_chg.battery_level =
            be32_to_cpu(rep_batt_chg.battery_level);

        rep_batt_chg.battery_voltage =
            be32_to_cpu(rep_batt_chg.battery_voltage);

        rep_batt_chg.battery_temp =
            be32_to_cpu(rep_batt_chg.battery_temp);

        printk(KERN_INFO "charger_status = %s, charger_type = %s,"
               " batt_status = %s, batt_level = %s,"
               " batt_volt = %u, batt_temp = %u,\n",
               charger_status[rep_batt_chg.charger_status],
               charger_type[rep_batt_chg.charger_type],
               battery_status[rep_batt_chg.battery_status],
               battery_level[rep_batt_chg.battery_level],
               rep_batt_chg.battery_voltage,
               rep_batt_chg.battery_temp);

    }
    else
    {
        printk(KERN_INFO "%s():No more data in batt_chg rpc reply\n",
               __func__);
        return -EIO;
    }

    return 0;
}


//weijiun_ling, no used
static void msm_batt_update_psy_status_v1(void)
{
    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    msm_batt_get_batt_chg_status_v1();

    if (msm_batt_info.charger_status == rep_batt_chg.charger_status &&
            msm_batt_info.charger_type == rep_batt_chg.charger_type &&
            msm_batt_info.battery_status ==  rep_batt_chg.battery_status &&
            msm_batt_info.battery_level ==  rep_batt_chg.battery_level &&
            msm_batt_info.battery_voltage  == rep_batt_chg.battery_voltage
            && msm_batt_info.battery_temp ==  rep_batt_chg.battery_temp)
    {

        printk(KERN_NOTICE "%s() : Got unnecessary event from Modem "
               "PMIC VBATT driver. Nothing changed in Battery or "
               "charger status\n", __func__);
        return;
    }

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    msm_batt_info.battery_voltage  	= 	rep_batt_chg.battery_voltage;
    msm_batt_info.battery_temp 	=	rep_batt_chg.battery_temp;


    if (rep_batt_chg.battery_status != BATTERY_STATUS_INVALID)
    {

        msm_batt_info.batt_valid = 1;

        if (msm_batt_info.battery_voltage >
                msm_batt_info.voltage_max_design)
        {

            msm_batt_info.batt_health =
                POWER_SUPPLY_HEALTH_OVERVOLTAGE;

            msm_batt_info.batt_status =
                POWER_SUPPLY_STATUS_NOT_CHARGING;

        }
        else if (msm_batt_info.battery_voltage
                 < msm_batt_info.voltage_min_design)
        {

            msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_DEAD;
            msm_batt_info.batt_status =
                POWER_SUPPLY_STATUS_UNKNOWN;

        }
        else if (rep_batt_chg.battery_status == BATTERY_STATUS_BAD)
        {

            msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_DEAD;
            msm_batt_info.batt_status = POWER_SUPPLY_STATUS_UNKNOWN;

        }
        else if (rep_batt_chg.battery_status ==
                 BATTERY_STATUS_BAD_TEMP)
        {

            msm_batt_info.batt_health =
                POWER_SUPPLY_HEALTH_OVERHEAT;

            if (rep_batt_chg.charger_status == CHARGER_STATUS_BAD
                    || rep_batt_chg.charger_status ==
                    CHARGER_STATUS_INVALID)
                msm_batt_info.batt_status =
                    POWER_SUPPLY_STATUS_UNKNOWN;
            else
                msm_batt_info.batt_status =
                    POWER_SUPPLY_STATUS_NOT_CHARGING;

        }
        else if ((rep_batt_chg.charger_status == CHARGER_STATUS_GOOD
                  || rep_batt_chg.charger_status == CHARGER_STATUS_WEAK)
                 && (rep_batt_chg.battery_status ==
                     BATTERY_STATUS_GOOD))
        {

            msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;

            if (rep_batt_chg.battery_level == BATTERY_LEVEL_FULL)
                msm_batt_info.batt_status =
                    POWER_SUPPLY_STATUS_FULL;
            else
                msm_batt_info.batt_status =
                    POWER_SUPPLY_STATUS_CHARGING;

        }
        else if ((rep_batt_chg.charger_status == CHARGER_STATUS_BAD
                  || rep_batt_chg.charger_status ==
                  CHARGER_STATUS_INVALID) &&
                 (rep_batt_chg.battery_status ==
                  BATTERY_STATUS_GOOD))
        {

            msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
            msm_batt_info.batt_status =
                POWER_SUPPLY_STATUS_DISCHARGING;
        }

        msm_batt_info.batt_capacity =
            msm_batt_info.calculate_capacity(
                msm_batt_info.battery_voltage);

    }
    else
    {
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_UNKNOWN;
        msm_batt_info.batt_status = POWER_SUPPLY_STATUS_UNKNOWN;
        msm_batt_info.batt_capacity = 0;
    }


    if (msm_batt_info.charger_type != rep_batt_chg.charger_type)
    {

        msm_batt_info.charger_type = rep_batt_chg.charger_type ;

        if (msm_batt_info.charger_type == CHARGER_TYPE_WALL)
        {

            msm_batt_info.current_chg_source &= ~USB_CHG;
            msm_batt_info.current_chg_source |= AC_CHG;

            printk(KERN_INFO "%s() : charger_type = WALL\n",
                   __func__);

            power_supply_changed(&msm_psy_ac);

        }
        else if (msm_batt_info.charger_type ==
                 CHARGER_TYPE_USB_WALL ||
                 msm_batt_info.charger_type ==
                 CHARGER_TYPE_USB_PC)
        {

            msm_batt_info.current_chg_source &= ~AC_CHG;
            msm_batt_info.current_chg_source |= USB_CHG;

            printk(KERN_INFO "%s() : charger_type = %s\n",
                   __func__,
                   charger_type[msm_batt_info.charger_type]);

            power_supply_changed(&msm_psy_usb);

        }
        else
        {

            printk(KERN_INFO "%s() : charger_type = %s\n",
                   __func__,
                   charger_type[msm_batt_info.charger_type]);

            msm_batt_info.batt_status =
                POWER_SUPPLY_STATUS_DISCHARGING;

            if (msm_batt_info.current_chg_source & AC_CHG)
            {

                msm_batt_info.current_chg_source &= ~AC_CHG;

                printk(KERN_INFO "%s() : AC WALL charger"
                       " removed\n", __func__);

                power_supply_changed(&msm_psy_ac);

            }
            else if (msm_batt_info.current_chg_source & USB_CHG)
            {

                msm_batt_info.current_chg_source &= ~USB_CHG;
                printk(KERN_INFO "%s() : USB charger removed\n",
                       __func__);

                power_supply_changed(&msm_psy_usb);
            }
            else
                power_supply_changed(&msm_psy_batt);
        }
    }
    else
        power_supply_changed(&msm_psy_batt);

    msm_batt_info.charger_status 	= 	rep_batt_chg.charger_status;
    msm_batt_info.charger_type 	= 	rep_batt_chg.charger_type;
    msm_batt_info.battery_status 	=  	rep_batt_chg.battery_status;
    msm_batt_info.battery_level 	=  	rep_batt_chg.battery_level;
    msm_batt_info.battery_voltage  	= 	rep_batt_chg.battery_voltage;
    msm_batt_info.battery_temp 	=	rep_batt_chg.battery_temp;
}


//weijiun_ling
static int Get_charging_uah(s64* val)
{

    int rc;
    s64 value;

    struct rpc_request_hdr req_batt_chg;

    struct rpc_reply_chg_reply
    {
        struct rpc_reply_hdr hdr;
        s64 chg_batt_data;
    } rep_chg;

    rc = msm_rpc_call_reply(msm_batt_info.cci_ep,
                            ONCRPC_CCI_GET_CHARGING_UAH,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "[Battery]: %s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CCI_GET_CHARGING_UAH, rc);
        return rc;
    }

    value = be64_to_cpu(rep_chg.chg_batt_data);
    *val = value;
    printk("[Battery]: Get_charging_uah, value:0x%x%x\n", (s32)(value >> 32), (s32)(value & 0xFFFFFFFF));

    return 0;


}

static int msm_batt_get_batt_rpc(
    u32 *batt_volt,
    u32 *batt_charging,
    u32 *charger_valid,
    u32 *chg_batt_event,
    s32 *chg_batt_temp,
    s32 *power_level_dbm,
    s32 *amss_chg_state
)
{
    struct rpc_request_hdr req_batt_chg;

    struct power_level_req
    {
        struct rpc_request_hdr hdr;
        u32 option;

    } dBm_req;

    struct rpc_reply_batt_volt
    {
        struct rpc_reply_hdr hdr;
        u32 voltage;
    } rep_volt;

    struct rpc_reply_chg_reply
    {
        struct rpc_reply_hdr hdr;
        s32 chg_batt_data;
    } rep_chg;

    int rc;
    u32 option_dbm = 0;
    //s32 power_level;
    cci_smem_value_t *smem_cci_smem_value;

    *batt_charging = 0;
    *chg_batt_event = CHG_UI_EVENT__INVALID;
    *charger_valid = 0;

    smem_cci_smem_value = smem_alloc( SMEM_CCI_SMEM_VALUE, sizeof( cci_smem_value_t ) );
    charger_state = smem_cci_smem_value->cci_set_chg_state;
    Ichg = (int)smem_cci_smem_value->cci_gauge_ic_profile_version;



    dBm_req.option = cpu_to_be32(option_dbm);

//--  RPC for BATTERY_READ_PROC --       //ONCRPC_PM_VBATT_READ_PROC
    rc = msm_rpc_call_reply(msm_batt_info.batt_ep,
                            BATTERY_READ_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_volt, sizeof(rep_volt),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, BATTERY_READ_PROC, rc);

        return rc;
    }
    *batt_volt = be32_to_cpu(rep_volt.voltage);
//-- END --


//--  RPC for ONCRPC_CHG_IS_CHARGING_PROC --
    rc = msm_rpc_call_reply(msm_batt_info.chg_ep,
                            ONCRPC_CHG_IS_CHARGING_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_IS_CHARGING_PROC, rc);
        return rc;
    }
    *batt_charging = be32_to_cpu(rep_chg.chg_batt_data);
//-- END --

//--  RPC for ONCRPC_CHG_IS_CHARGER_VALID_PROC --     //weijiun_ling, return chg_is_charger_valid_flag
    rc = msm_rpc_call_reply(msm_batt_info.chg_ep,
                            ONCRPC_CHG_IS_CHARGER_VALID_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_IS_CHARGER_VALID_PROC, rc);
        return rc;
    }
    *charger_valid = be32_to_cpu(rep_chg.chg_batt_data);
//-- END --

//--  RPC for ONCRPC_CHG_IS_BATTERY_VALID_PROC --
    rc = msm_rpc_call_reply(msm_batt_info.chg_ep,
                            ONCRPC_CHG_IS_BATTERY_VALID_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_IS_BATTERY_VALID_PROC, rc);
        return rc;
    }
    msm_batt_info.batt_valid = be32_to_cpu(rep_chg.chg_batt_data);
//-- END --

//--  RPC for ONCRPC_CHG_UI_EVENT_READ_PROC --    weijiun_ling, get modem's event
    rc = msm_rpc_call_reply(msm_batt_info.chg_ep,
                            ONCRPC_CHG_UI_EVENT_READ_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_UI_EVENT_READ_PROC, rc);
        return rc;
    }

        *chg_batt_event = be32_to_cpu(rep_chg.chg_batt_data);
//-- END --


//bincent
//--  RPC for ONCRPC_CHG_GET_TEMP_PROC --   //weijiun_ling, battery temperature
#if 1
    rc = msm_rpc_call_reply(msm_batt_info.cci_ep,
                            ONCRPC_CHG_GET_TEMP_PROC,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CHG_GET_TEMP_PROC, rc);
        return rc;
    }

    *chg_batt_temp = be32_to_cpu(rep_chg.chg_batt_data);
#else
    *chg_batt_temp = 0;
#endif
//-- END --

//--  RPC for ONCPRC_CCI_READ_TX_POWER_PROC --
    //power level  (Change charging table when talking)
    rc = msm_rpc_call_reply(msm_batt_info.cci_ep,
                            ONCPRC_CCI_READ_TX_POWER_PROC,
                            &dBm_req, sizeof(dBm_req),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCPRC_CCI_READ_TX_POWER_PROC, rc);
        return rc;
    }
    *power_level_dbm = be32_to_cpu(rep_chg.chg_batt_data);
    //printk("power_level = %d\n",power_level);
//-- END --

//--  RPC for ONCRPC_CCI_GET_AMSS_CHG_STATE --
    rc = msm_rpc_call_reply(msm_batt_info.cci_ep,
                            ONCRPC_CCI_GET_AMSS_CHG_STATE,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CCI_GET_AMSS_CHG_STATE, rc);
        return rc;
    }

    *amss_chg_state = be32_to_cpu(rep_chg.chg_batt_data);

#ifdef BATT_FOR_TEST_OT
//[TEST]
    if (counter > 0)
    {
        if (counter > 2 && counter < 6)
            *chg_batt_event = CHG_UI_EVENT__BAD_TEMP;
        else
        {
            *chg_batt_event = CHG_UI_EVENT__NORMAL_POWER;
        }

        if (counter > 7)
            counter = 1;
        else
            counter++;
    }
    printk("[Battery]: counter:%d\n\n\n",counter);
//[TEST]
#endif

//-- END --
    return 0;
}

//weijiun_ling
#define MAX_CAP_MA     1130
#define CONV_PERCENT_TO_uAH(p)    ((p * MAX_CAP_MA * 1000) / 100)
#define THEO_uAS(mA,sec)  ((mA) * 1000 * (sec))
#define MAX_uAH   CONV_PERCENT_TO_uAH(100)
#define Val_99_uAH    CONV_PERCENT_TO_uAH(99)


static void Smooth_Cap(int *ACap, int *CCap, int Chg_Type)
{
    s64 Local_Adc_uAH = CONV_PERCENT_TO_uAH(*ACap);
    s64 Delta_uAH = Local_Adc_uAH - T_Capacity_uAH;
    s64 THEO_uAH = 0;
    s64 rtn = 0;
    short Local_Ichg = 0;
    short Adj_level = 0;

    if (ACap == NULL || CCap == NULL)
    {
        printk(KERN_ERR "ACap or CCap is NULL\n");
        return;
    }

    //Boundary check for Ichg
    Local_Ichg = Ichg - (cci_cal_current);

    if (Local_Ichg > 1000)
        Local_Ichg = 1000;
    else if (Local_Ichg < 0)
        Local_Ichg = 0;

    if (Chg_Type > NO_CHG)
    {
        //printk("[Battery]: Local_Ichg:%d\n", Local_Ichg);
        if (charger_flag != CHG_COMPLETE && T_Capacity_uAH >= Val_99_uAH)
        {
            if (msm_batt_info.batt_capacity < 100)    //check prev. capacity was lower than 100
                T_Capacity_uAH = Val_99_uAH;
        }
        else if (charger_state == CCI_CHG_SET_CC_MODE)
        {
            if (Delta_uAH > (-1) * CONV_PERCENT_TO_uAH(20))
            {
                THEO_uAH = THEO_uAS(Local_Ichg, 30);
                do_div(THEO_uAH, 3600);
                T_Capacity_uAH = T_Capacity_uAH + THEO_uAH;
            }
        }
        else
        {
            if (Delta_uAH > (-1) * CONV_PERCENT_TO_uAH(10))
            {
                THEO_uAH = THEO_uAS(Local_Ichg, 30);
                do_div(THEO_uAH, 3600);
                T_Capacity_uAH = T_Capacity_uAH + THEO_uAH;
            }
        }
    }
    else
    {
        if (Delta_uAH < 0)
        {
            THEO_uAH = THEO_uAS(cci_cal_current, 30);
            do_div(THEO_uAH, 3600);
            T_Capacity_uAH = T_Capacity_uAH - THEO_uAH;

            Adj_level = (short)(abs(Delta_uAH) / CONV_PERCENT_TO_uAH(5));
            if (Adj_level > 0 && msm_batt_info.batt_capacity < 100)
            {
                T_Capacity_uAH = T_Capacity_uAH - THEO_uAH * Adj_level;
                //printk("[Battery]: Adjust estimate currente, Adj_level:%d\n",Adj_level);
            }
        }
    }

    //Boundary check
    if (T_Capacity_uAH < 0)
        T_Capacity_uAH = 0;
    else if (T_Capacity_uAH > MAX_uAH)
        T_Capacity_uAH = MAX_uAH;


    //rounding off
    rtn = (T_Capacity_uAH + 5 * MAX_CAP_MA);    //T_Capacity_uAH + 0.5 * 10 * MAX_CAP_MA
    do_div(rtn, (10 * MAX_CAP_MA));     //(T_Capacity_uAH * 100 / 1000 / MAX_CAP_MA)
    if (rtn > 100)
        rtn = 100;

    *CCap  = (u32)rtn;


}

static void Smooth_Cap_Resume(int *ACap, int *CCap, u32 suspend_time)
{
    s64 chg_uah = 0;
    s64 Local_Adc_uAH = 0;
    s64 rtn = 0;
    s64 THEO_uAH = 0;
    int rc = 0;
    int Local_IsOnCall = atomic_read(&g_IsOnCall);

    rc = Get_charging_uah(&chg_uah);
    if (rc < 0)    //Can't get rpc from modem
    {
        *CCap = *ACap;
        return;
    }

    Local_Adc_uAH = CONV_PERCENT_TO_uAH(*ACap);

    if (charger_flag == CHG_COMPLETE)
    {
        T_Capacity_uAH = MAX_uAH;
    }
    else if (chg_uah > 0 || charger_flag == CHG_OT)
    {
        T_Capacity_uAH += chg_uah;
    }
    else if (g_Suspend_Chg_Source == NO_CHG)
    {
        if (Local_IsOnCall)     //On call after suspend
        {
            if (Local_Adc_uAH <= T_Capacity_uAH)
                T_Capacity_uAH = Local_Adc_uAH;    //using adc's value
        }        
        else if (suspend_time < 19*60)    //Discharged less than 19 min or not a phone call during suspend
        {
            {
                THEO_uAH = THEO_uAS(10, suspend_time);
                do_div(THEO_uAH, 3600);
                T_Capacity_uAH = T_Capacity_uAH - THEO_uAH;
            }
        }
        else
        {
            if (Local_Adc_uAH <= T_Capacity_uAH)
                T_Capacity_uAH = Local_Adc_uAH;    //using adc's value
        }
        printk("[Battery]: Smooth_Cap_Resume, suspend_time:%d, IsOnCall:%d\n",suspend_time, Local_IsOnCall);

    }
    else if (chg_uah == 0 && g_Suspend_Chg_Source == AC_CHG)  //To avoid plugging charger then resume rapidly
    {
        T_Capacity_uAH += chg_uah;
    }
    else
    {
        if (Local_Adc_uAH <= T_Capacity_uAH)
            T_Capacity_uAH = Local_Adc_uAH;    //using adc's value
    }

    //Boundary check
    if (T_Capacity_uAH < 0)
        T_Capacity_uAH = 0;
    else if (T_Capacity_uAH > MAX_uAH)
        T_Capacity_uAH = MAX_uAH;

    //rounding off
    rtn = (T_Capacity_uAH + 5 * MAX_CAP_MA);    //T_Capacity_uAH + 0.5 * 10 * MAX_CAP_MA
    do_div(rtn, (10 * MAX_CAP_MA));     //(T_Capacity_uAH * 100 / 1000 / MAX_CAP_MA)
    if (rtn > 100)
        rtn = 100;

    //Sync with modem's charge state
    if (rtn == 100 && msm_batt_info.current_chg_source > NO_CHG
            && msm_batt_info.batt_capacity != 100 && charger_flag != CHG_COMPLETE )
    {
        rtn = 99;
        T_Capacity_uAH = CONV_PERCENT_TO_uAH(99);
    }

    *CCap  = (u32)rtn;


}

//[Bug 927] Add the mechanism of RPC error recovery, weijiun_ling, 2010.11.01
static void Batt_RPC_Error_Handle(
    u32 *batt_volt,
    u32 *chg_batt_event,
    s32 *chg_batt_temp)
{
    *batt_volt = msm_batt_info.battery_voltage;
    *chg_batt_event = g_Chg_UI_event;
    *chg_batt_temp = msm_batt_info.battery_temp;
    printk("[Battery]: %s(), v:%d,  M_evt:%u, T:%d \n", __func__,
		msm_batt_info.battery_voltage, g_Chg_UI_event, msm_batt_info.battery_temp);
}
//End

static void msm_batt_update_psy_status_v0(void)
{
    u32 batt_charging = 0;
    u32 batt_volt = 0;
    u32 chg_batt_event = CHG_UI_EVENT__INVALID;
    s32 chg_batt_temp = 0;//over temp
    s32 power_level_dbm = 0;
    u32 charger_valid = 0;
    s32 localpercent;
    s32 amss_chg_state;
    u32 localvoltage = 0;
    s32 cci_cal_current_diff = 0;

//weijiun_ling
    int CCapacity = 0;
    int rc = 0;


//printk(KERN_INFO "[Battery]: %s()\n", __func__);

    rc = msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);

//[Bug 927] Add the mechanism of RPC error recovery, weijiun_ling, 2010.11.01
    if (rc < 0)
    {
        //Retry again
        msleep(500);
        rc = msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);
        if (rc < 0)
            Batt_RPC_Error_Handle(&batt_volt, &chg_batt_event, &chg_batt_temp);
    }
//End

    msm_batt_info.charger_valid = charger_valid;

    /* Calcuate the estimate of current */
    Estimate_Current(power_level_dbm);

    Check_FieldTest(&chg_batt_event);
    UI_To_UpperLayer(chg_batt_event);

    //if (msm_batt_info.batt_valid) {

    if (msm_batt_info.voltage_now > msm_batt_info.voltage_max_design)
    {
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    }
    else if (msm_batt_info.voltage_now < msm_batt_info.voltage_min_design)
    {
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_DEAD;
    }
    else
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;

    localvoltage = batt_volt;

    {

        if (call_state == ENABLE_BATT_DBG_INFO)
            printk("cci_cal_current = %d ,cci_batt_device_mask = 0x%x\n",cci_cal_current,cci_batt_device_mask);

        cci_cal_current_diff = cci_cal_current - pre_cci_cal_current;
        if (call_state == ENABLE_BATT_DBG_INFO)
            printk("cci_cal_current_diff = %d\n",cci_cal_current_diff );

    }

    //batt_temp
    msm_batt_info.batt_temp = chg_batt_temp;

    //Get battery percentage
    localpercent = msm_batt_capacity(localvoltage);

    if (T_Capacity_uAH == -12345678)   //For initialize
    {
        T_Capacity_uAH =  CONV_PERCENT_TO_uAH(localpercent);
        msm_batt_info.batt_capacity = localpercent;
    }

//weijiun_ling
    if (chg_batt_event < CHG_UI_EVENT__BAD_TEMP)
    {
        //Convert % unit to uA
        if (T_Capacity_uAH > MAX_uAH || chg_batt_event == CHG_UI_EVENT__DONE)
        {
            T_Capacity_uAH = MAX_uAH;
            msm_batt_info.batt_capacity = 100;
        }
        else if (T_Capacity_uAH > 0)
        {
            Smooth_Cap(&localpercent, &CCapacity,  msm_batt_info.current_chg_source);
            printk("[Battery]: Adc_Capacity:%d, CCapacity:%d\n", localpercent, CCapacity);
            localpercent = CCapacity;
        }
    }


    //Detect battery capacity, if battery capacity = 0%, power off the system
    if (localpercent > 0)
    {
        if (atomic_read(&g_ChgVld))
        {
            if (localpercent <  msm_batt_info.batt_capacity )
                localpercent = msm_batt_info.batt_capacity;
        }
        else
        {
            if (localpercent > msm_batt_info.batt_capacity)
                localpercent = msm_batt_info.batt_capacity;
        }
        g_Shotdown_Count = 1;
    }
    else if (msm_batt_info.current_chg_source == NO_CHG) //if device is not charging
    {
        printk("[Battery]: BattLifeNotGood! LifePercent = %d, Count:%d\n", localpercent, g_Shotdown_Count);

        if (g_Shotdown_Count < 1)
        {
            localpercent = 0;
        }
        else
        {
            g_Shotdown_Count--;
            localpercent = 1;
        }
    }


    msm_batt_info.batt_capacity = localpercent;
    msm_batt_info.voltage_now = batt_volt;

    g_Resume_Capacity = msm_batt_info.batt_capacity;

    printk("[Battery]: M_Chgr:%d, Flg:%d, Ichg:%d, Call_state:%d, M_Chg:%d, C:%d, msk:0x%x\n",
           charger_state, charger_flag, Ichg, call_state, amss_chg_state, cci_cal_current,cci_batt_device_mask);

    printk("[Battery]: IsChg:%u, b_vld:%u, c_vld:%u, LF:%d, V:%d, T:%d,"
           " M_evt:%u, PWR:%d, g_chgSrc:%d\n",
           batt_charging, msm_batt_info.batt_valid,charger_valid,
           msm_batt_info.batt_capacity, msm_batt_info.voltage_now,msm_batt_info.batt_temp,
           chg_batt_event,power_level_dbm,msm_batt_info.current_chg_source);

//[Bug 807] Add battery log on framework layer, weijiun_ling, 2010.10.13
#ifdef BATTERY_LOGCAT
    sprintf(dbglog,"LF:%d, V:%d, T:%d, M_Chg:%d, M_evt:%u, M_Chgr:%d, Flg:%d, g_chgSrc:%d, rc:%d\n",
		msm_batt_info.batt_capacity, msm_batt_info.voltage_now,
		msm_batt_info.batt_temp, amss_chg_state, chg_batt_event,
		charger_state, charger_flag, msm_batt_info.current_chg_source,rc);
#endif
//End

    power_supply_changed(&msm_psy_batt);
}

static void msm_batt_update_psy_status_v2(void)
{
    u32 batt_charging = 0;
    u32 batt_volt = 0;
    u32 chg_batt_event = CHG_UI_EVENT__INVALID;
    s32 chg_batt_temp = 0;//over temp
    s32 power_level_dbm = 0;
    u32 charger_valid = 0;
    s32 localpercent;
    s32 amss_chg_state = 0;

    u32 localvoltage = 0;
    s32 cci_cal_current_diff = 0;
    int rc = 0;



    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    rc = msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);

//[Bug 927] Add the mechanism of RPC error recovery, weijiun_ling, 2010.11.01
    if (rc < 0)
    {
        //Retry again twice
        msleep(500);
        rc = msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);
        if (rc < 0)
        {
            msleep(500);
            rc = msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);
        }
    }
//End

    msm_batt_info.charger_valid = charger_valid;


    Estimate_Current(power_level_dbm);

    Check_FieldTest(&chg_batt_event);
    UI_To_UpperLayer(chg_batt_event);

    if (msm_batt_info.voltage_now > msm_batt_info.voltage_max_design)
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
    else if (msm_batt_info.voltage_now < msm_batt_info.voltage_min_design)
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_DEAD;
    else
        msm_batt_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;

    {


        if (call_state == ENABLE_BATT_DBG_INFO)
            printk("cci_cal_current = %d ,cci_batt_device_mask = 0x%x\n",cci_cal_current,cci_batt_device_mask);
        cci_cal_current_diff = cci_cal_current - pre_cci_cal_current;
        if (call_state == ENABLE_BATT_DBG_INFO)
            printk("cci_cal_current_diff = %d\n",cci_cal_current_diff );

    }



    //batt_temp
    msm_batt_info.batt_temp = chg_batt_temp;

    //localvoltage = msm_batt_info.voltage_now;  //[BugID:553]
    localvoltage = batt_volt;
    localpercent = msm_batt_capacity(localvoltage);


//weijiun_ling
    //msm_batt_info.batt_capacity = localpercent;  //Save adc's capacity to batt_capacity
    g_Resume_Capacity = localpercent;  //Save adc's capacity to batt_capacity
    //power_supply_changed(&msm_psy_batt);      //Delay updating power status at late resume

    g_Is_Run_suspend = true;

    printk("[Battery]: M_Chgr:%d, Flg:%d, Ichg:%d, Call_state:%d, M_Chg:%d, C:%d, msk:0x%x\n",
           charger_state, charger_flag, Ichg, call_state, amss_chg_state, cci_cal_current,cci_batt_device_mask);

    printk("[Battery]: IsChg:%u, b_vld:%u, c_vld:%u, LF:%d, V:%d, T:%d,"
           " M_evt:%u, PWR:%d, g_chgSrc:%d, g_rsm_LF:%d\n",
           batt_charging, msm_batt_info.batt_valid,charger_valid,
           msm_batt_info.batt_capacity, batt_volt,msm_batt_info.batt_temp,
           chg_batt_event,power_level_dbm,msm_batt_info.current_chg_source, g_Resume_Capacity);
	
}

static void msm_batt_update_psy_status_v3(void)
{

//    int i;
    struct timespec end_ts;
//    struct rtc_time end_tm;
//    unsigned  smooth=0;
    unsigned int elapsed_sec =0;
//    unsigned int elapsed_hour =0;


    int CCapacity = 0;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    //smooth capacity linear due to resume phone
    getnstimeofday(&end_ts);
    //rtc_time_to_tm(end_ts.tv_sec, &end_tm);

    if (end_ts.tv_sec - pre_ts.tv_sec >= 0)
    {
        elapsed_sec = end_ts.tv_sec - pre_ts.tv_sec;
        printk("[Battery]: Suspend_sec:%d\n", elapsed_sec);
    }
    else
    {
        printk("[Battery]: Delta time overflow\n");
    }

    //It will not suspend while usb plugging
    if (g_Is_Run_suspend)
    {
        Smooth_Cap_Resume(&g_Resume_Capacity, &CCapacity, elapsed_sec);
        msm_batt_info.batt_capacity = CCapacity;
    }

//[BugID:607] [BSP] Move to USB at dormancy estate find can't unlock
    getnstimeofday(&pre_ts);

    printk("[Battery]: Late resume capacity:%d, chg_source:%d, g_Suspend_Chg_Source:%d, Is_Sleep:%d \n",
           msm_batt_info.batt_capacity, msm_batt_info.current_chg_source, g_Suspend_Chg_Source, g_Is_Run_suspend);

    //Reset g_Suspend_Chg_Source
    g_Suspend_Chg_Source = msm_batt_info.current_chg_source;

    g_Is_Run_suspend = false;

    power_supply_changed(&msm_psy_batt);


}


/*
static void msm_batt_update_complete(void)
{
    //msm_batt_get_batt_chg_status1();
    msm_batt_info.batt_status = POWER_SUPPLY_STATUS_FULL;
    msm_batt_info.batt_capacity = 100;
    //charger_flag = 2;	//add for wayne
    //msm_batt_info.calculate_capacity(msm_batt_info.voltage_now);
    printk(KERN_INFO "[Battery]: %s()\n", __func__);
    power_supply_changed(&msm_psy_batt);
}
*/

static int msm_batt_register(u32 desired_batt_voltage,
                             u32 voltage_direction, u32 batt_cb_id, u32 cb_data)
{
    struct batt_client_registration_req req;
    struct batt_client_registration_rep rep;
    int rc;

    req.desired_batt_voltage = cpu_to_be32(desired_batt_voltage);
    req.voltage_direction = cpu_to_be32(voltage_direction);
    req.batt_cb_id = cpu_to_be32(batt_cb_id);
    req.cb_data = cpu_to_be32(cb_data);
    req.more_data = cpu_to_be32(1);
    req.batt_error = cpu_to_be32(0);

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    rc = msm_rpc_call_reply(msm_batt_info.batt_ep,
                            BATTERY_REGISTER_PROC, &req,
                            sizeof(req), &rep, sizeof(rep),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR
               "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, BATTERY_REGISTER_PROC, rc);
        return rc;
    }
    else
    {
        rc = be32_to_cpu(rep.batt_clnt_handle);
        printk(KERN_INFO "batt_clnt_handle = %d\n", rc);
        return rc;
    }
}

static int msm_batt_modify_client(u32 client_handle, u32 desired_batt_voltage,
                                  u32 voltage_direction, u32 batt_cb_id, u32 cb_data)
{
    int rc;

    struct batt_modify_client_req
    {
        struct rpc_request_hdr hdr;

        u32 client_handle;

        /* The voltage at which callback (CB) should be called. */
        u32 desired_batt_voltage;

        /* The direction when the CB should be called. */
        u32 voltage_direction;

        /* The registered callback to be called when voltage and
         * direction specs are met. */
        u32 batt_cb_id;

        /* The call back data */
        u32 cb_data;
    } req;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    req.client_handle = cpu_to_be32(client_handle);
    req.desired_batt_voltage = cpu_to_be32(desired_batt_voltage);
    req.voltage_direction = cpu_to_be32(voltage_direction);
    req.batt_cb_id = cpu_to_be32(batt_cb_id);
    req.cb_data = cpu_to_be32(cb_data);

    msm_rpc_setup_req(&req.hdr, BATTERY_RPC_PROG, BATTERY_RPC_VERS,
                      BATTERY_MODIFY_CLIENT_PROC);

    msm_batt_info.vbatt_rpc_req_xid = req.hdr.xid;

    rc = msm_rpc_write(msm_batt_info.batt_ep, &req, sizeof(req));

    if (rc < 0)
    {
        printk(KERN_ERR
               "%s(): msm_rpc_write failed.  proc = 0x%08x rc = %d\n",
               __func__, BATTERY_MODIFY_CLIENT_PROC, rc);
        return rc;
    }

    return 0;
}

static int msm_batt_deregister(u32 handle)
{
    int rc;
    struct batt_client_deregister_req
    {
        struct rpc_request_hdr req;
        s32 handle;
    } batt_deregister_rpc_req;

    struct batt_client_deregister_reply
    {
        struct rpc_reply_hdr hdr;
        u32 batt_error;
    } batt_deregister_rpc_reply;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    batt_deregister_rpc_req.handle = cpu_to_be32(handle);
    batt_deregister_rpc_reply.batt_error = cpu_to_be32(BATTERY_LAST_ERROR);

    rc = msm_rpc_call_reply(msm_batt_info.batt_ep,
                            BATTERY_DEREGISTER_CLIENT_PROC,
                            &batt_deregister_rpc_req,
                            sizeof(batt_deregister_rpc_req),
                            &batt_deregister_rpc_reply,
                            sizeof(batt_deregister_rpc_reply),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR
               "%s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, BATTERY_DEREGISTER_CLIENT_PROC, rc);
        return rc;
    }

    if (be32_to_cpu(batt_deregister_rpc_reply.batt_error) !=
            BATTERY_DEREGISTRATION_SUCCESSFUL)
    {

        printk(KERN_ERR "%s: vBatt deregistration Failed "
               "  proce_num = %d,"
               " batt_clnt_handle = %d\n",
               __func__, BATTERY_DEREGISTER_CLIENT_PROC, handle);
        return -EIO;
    }
    return 0;
}

static int  msm_batt_handle_suspend(void)
{
    int rc;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    if (msm_batt_info.batt_handle != INVALID_BATT_HANDLE)
    {

        printk(KERN_INFO "[Battery]: %s(), cal msm_batt_modify_client \n", __func__);
        rc = msm_batt_modify_client(msm_batt_info.batt_handle,
                                    BATTERY_LOW, BATTERY_VOLTAGE_BELOW_THIS_LEVEL,
                                    BATTERY_CB_ID_LOW_VOL, BATTERY_LOW);

        if (rc < 0)
        {
            printk(KERN_ERR
                   "%s(): failed to modify client for registering"
                   " call back when  voltage goes below %u\n",
                   __func__, BATTERY_LOW);

            return rc;
        }
    }

    return 0;
}

static int  msm_batt_handle_resume(void)
{
    int rc;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    if (msm_batt_info.batt_handle != INVALID_BATT_HANDLE)
    {

        printk(KERN_INFO "[Battery]: %s(), call msm_batt_modify_client \n", __func__);

        rc = msm_batt_modify_client(msm_batt_info.batt_handle,
                                    BATTERY_LOW, BATTERY_ALL_ACTIVITY,
                                    BATTERY_CB_ID_ALL_ACTIV, BATTERY_ALL_ACTIVITY);
        if (rc < 0)
        {
            printk(KERN_ERR
                   "%s(): failed to modify client for registering"
                   " call back for ALL activity \n", __func__);
            return rc;
        }
    }
    return 0;
}


static int  msm_batt_handle_event(void)
{
    int rc;

    if (!atomic_read(&msm_batt_info.handle_event))
    {

        printk(KERN_ERR "%s(): batt call back thread while in "
               "msm_rpc_read got signal. Signal is not from "
               "early suspend or  from late resume or from Clean up "
               "thread.\n", __func__);
        return 0;
    }

    printk(KERN_INFO "%s(): batt call back thread while in msm_rpc_read "
           "got signal\n", __func__);

    printk(KERN_INFO "[Battery]: %s()\n", __func__);


    if (msm_batt_info.type_of_event & SUSPEND_EVENT)
    {

        printk(KERN_INFO "%s(): Handle Suspend event. event = %08x\n",
               __func__, msm_batt_info.type_of_event);

        rc = msm_batt_handle_suspend();

        return rc;

    }
    else if (msm_batt_info.type_of_event & RESUME_EVENT)
    {

        printk(KERN_INFO "%s(): Handle Resume event. event = %08x\n",
               __func__, msm_batt_info.type_of_event);

        rc = msm_batt_handle_resume();

        return rc;

    }
    else if (msm_batt_info.type_of_event & CLEANUP_EVENT)
    {

        printk(KERN_INFO "%s(): Cleanup event occured. event = %08x\n",
               __func__, msm_batt_info.type_of_event);

        return 0;

    }
    else
    {

        printk(KERN_ERR "%s(): Unknown event occured. event = %08x\n",
               __func__, msm_batt_info.type_of_event);
        return 0;
    }
}


/*
static void msm_batt_handle_vbatt_rpc_reply(struct rpc_reply_hdr *reply)
{

	struct rpc_reply_vbatt_modify_client {
		struct rpc_reply_hdr hdr;
		u32 modify_client_result;
	} *rep_vbatt_modify_client;

	u32 modify_client_result;

	if (msm_batt_info.type_of_event & SUSPEND_EVENT) {
		printk(KERN_INFO "%s(): Suspend event. Got RPC REPLY for vbatt"
			" modify client RPC req. \n", __func__);
	} else if (msm_batt_info.type_of_event & RESUME_EVENT) {
		printk(KERN_INFO "%s(): Resume event. Got RPC REPLY for vbatt"
			" modify client RPC req. \n", __func__);
	}

	 If an earlier call timed out, we could get the (no longer wanted)
	  reply for it. Ignore replies that  we don't expect.

	if (reply->xid != msm_batt_info.vbatt_rpc_req_xid) {

		printk(KERN_ERR "%s(): returned RPC REPLY XID is not"
				" equall to VBATT RPC REQ XID \n", __func__);
		kfree(reply);

		return;
	}
	if (reply->reply_stat != RPCMSG_REPLYSTAT_ACCEPTED) {

		printk(KERN_ERR "%s(): reply_stat != "
			" RPCMSG_REPLYSTAT_ACCEPTED \n", __func__);
		kfree(reply);

		return;
	}

	if (reply->data.acc_hdr.accept_stat != RPC_ACCEPTSTAT_SUCCESS) {

		printk(KERN_ERR "%s(): reply->data.acc_hdr.accept_stat "
				" != RPCMSG_REPLYSTAT_ACCEPTED \n", __func__);
		kfree(reply);

		return;
	}

	rep_vbatt_modify_client =
		(struct rpc_reply_vbatt_modify_client *) reply;

	modify_client_result =
		be32_to_cpu(rep_vbatt_modify_client->modify_client_result);

	if (modify_client_result != BATTERY_MODIFICATION_SUCCESSFUL) {

		printk(KERN_ERR "%s() :  modify client failed."
			"modify_client_result  = %u\n", __func__,
			modify_client_result);
	} else {
		printk(KERN_INFO "%s() : modify client successful.\n",
				__func__);
	}

	kfree(reply);
}

*/

//weijiun_ling, no used
static void msm_batt_wake_up_waiting_thread(u32 event)
{
    msm_batt_info.type_of_event &= ~event;

    atomic_set(&msm_batt_info.handle_event, 0);
    atomic_set(&msm_batt_info.event_handled, 1);
    wake_up(&msm_batt_info.wait_q);

    printk(KERN_INFO "[Battery]: %s(), evnet:%d\n", __func__,event);
}


static void msm_batt_wait_for_batt_chg_event(struct work_struct *work)
{

//    printk(KERN_INFO "%s: Batt RPC call back thread started, api_ver:0x%X\n",
//		__func__,msm_batt_info.chg_api_version);


    if (msm_batt_info.chg_api_version >= CHARGER_API_VERSION)
    {
        msm_batt_handle_event();
        msm_batt_wake_up_waiting_thread(SUSPEND_EVENT | RESUME_EVENT);
        msm_batt_update_psy_status_v1();  //weijiun_ling, no used
    }
    else
        msm_batt_update_psy_status_v0(); //bincent Refresh_timer


    schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(30*1000));



}

static int msm_batt_send_event(u32 type_of_event)
{
    int rc;
    unsigned long flags;

    rc = 0;

    spin_lock_irqsave(&msm_batt_info.lock, flags);

    printk(KERN_INFO "[Battery]: %s(), type:0x%X \n", __func__,type_of_event);

    if (type_of_event & SUSPEND_EVENT)
        printk(KERN_INFO "%s() : Suspend event ocurred."
               "events = %08x\n", __func__, type_of_event);
    else if (type_of_event & RESUME_EVENT)
        printk(KERN_INFO "%s() : Resume event ocurred."
               "events = %08x\n", __func__, type_of_event);
    else if (type_of_event & CLEANUP_EVENT)
        printk(KERN_INFO "%s() : Cleanup event ocurred."
               "events = %08x\n", __func__, type_of_event);
    else
    {
        printk(KERN_ERR "%s() : Unknown event ocurred."
               "events = %08x\n", __func__, type_of_event);

        spin_unlock_irqrestore(&msm_batt_info.lock, flags);
        return -EIO;
    }

    msm_batt_info.type_of_event |=  type_of_event;

    if (msm_batt_info.cb_thread)
    {
        atomic_set(&msm_batt_info.handle_event, 1);
        send_sig(SIGCONT, msm_batt_info.cb_thread, 0);
        spin_unlock_irqrestore(&msm_batt_info.lock, flags);

        rc = wait_event_interruptible(msm_batt_info.wait_q,
                                      atomic_read(&msm_batt_info.event_handled));

        if (rc == -ERESTARTSYS)
        {

            printk(KERN_ERR "%s(): Suspend/Resume/cleanup thread "
                   "got a signal while waiting for batt call back"
                   " thread to finish\n", __func__);

        }
        else if (rc < 0)
        {

            printk(KERN_ERR "%s(): Suspend/Resume/cleanup thread "
                   "wait returned error while waiting for batt "
                   "call back thread to finish. rc = %d\n",
                   __func__, rc);
        }
        else
            printk(KERN_INFO "%s(): Suspend/Resume/cleanup thread "
                   "wait returned rc = %d\n", __func__, rc);

        atomic_set(&msm_batt_info.event_handled, 0);
    }
    else
    {
        printk(KERN_INFO "%s(): Battery call Back thread not Started.",
               __func__);

        atomic_set(&msm_batt_info.handle_event, 1);
        spin_unlock_irqrestore(&msm_batt_info.lock, flags);
    }

    return rc;
}

static void msm_batt_start_cb_thread(void)
{
    /*atomic_set(&msm_batt_info.handle_event, 0);
    atomic_set(&msm_batt_info.event_handled, 0);
    queue_work(msm_batt_info.msm_batt_wq, &msm_batt_cb_work);*/
    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    //[BugID:505] [BSP][BATT] To fix the problem of getting first battery info
    schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(30*1000)); 
}

static int msm_batt_cleanup(void)
{
    int rc = 0;
    int rc_local;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    if (msm_batt_info.msm_batt_wq)
    {
        msm_batt_send_event(CLEANUP_EVENT);
        destroy_workqueue(msm_batt_info.msm_batt_wq);
    }

    if (msm_batt_info.batt_handle != INVALID_BATT_HANDLE)
    {

        rc = msm_batt_deregister(msm_batt_info.batt_handle);
        if (rc < 0)
            printk(KERN_ERR
                   "%s: msm_batt_deregister failed rc=%d\n",
                   __func__, rc);
    }
    msm_batt_info.batt_handle = INVALID_BATT_HANDLE;

    if (msm_batt_info.msm_psy_ac)
        power_supply_unregister(msm_batt_info.msm_psy_ac);

    if (msm_batt_info.msm_psy_usb)
        power_supply_unregister(msm_batt_info.msm_psy_usb);
    if (msm_batt_info.msm_psy_batt)
        power_supply_unregister(msm_batt_info.msm_psy_batt);

    if (msm_batt_info.batt_ep)
    {
        rc_local = msm_rpc_close(msm_batt_info.batt_ep);
        if (rc_local < 0)
        {
            printk(KERN_ERR
                   "%s: msm_rpc_close failed for batt_ep rc=%d\n",
                   __func__, rc_local);
            if (!rc)
                rc = rc_local;
        }
    }

    if (msm_batt_info.chg_ep)
    {
        rc_local = msm_rpc_close(msm_batt_info.chg_ep);
        if (rc_local < 0)
        {
            printk(KERN_ERR
                   "%s: msm_rpc_close failed for chg_ep rc=%d\n",
                   __func__, rc_local);
            if (!rc)
                rc = rc_local;
        }
    }
    //bincent add
    if (msm_batt_info.cci_ep)
    {
        rc_local = msm_rpc_close(msm_batt_info.cci_ep);
        if (rc_local < 0)
        {
            printk(KERN_ERR
                   "%s: msm_rpc_close failed for cci_ep rc=%d\n",
                   __func__, rc_local);
            if (!rc)
                rc = rc_local;
        }
    }


#ifdef CONFIG_HAS_EARLYSUSPEND
    if (msm_batt_info.early_suspend.suspend == msm_batt_early_suspend)
        unregister_early_suspend(&msm_batt_info.early_suspend);
#endif
    return rc;
}


static int Scale_To_Capacity (short tIndex, int index, int vbatt_now, int vbatt_scale_min ,int vbatt_scale_max)
{
    int result;

    printk(KERN_INFO "[Battery]: %s(), tIndex:%d, i:%d, vnow:%d, vsmin:%d, vsmax:%d\n",
           __func__,tIndex, index,vbatt_now,vbatt_scale_min,vbatt_scale_max);

    if (tIndex == up_table_volt_25_usb_new)   //if (battery_scale == 9)
    {
        if ( vbatt_now > 4100)
            return 90;
        else
            return ((vbatt_now - vbatt_scale_min) *10 / (vbatt_scale_max - vbatt_scale_min))  + (index*10);
    }
    else if (tIndex == up_table_volt_25_ac_800)  //else if (battery_scale == 7)
    {
        if ( vbatt_now > 4100)
            return 70;
        else
            return ((vbatt_now - vbatt_scale_min) *5 / (vbatt_scale_max - vbatt_scale_min))  + (index*5);
    }
    else if (tIndex == up_table_volt_25_ac_400)    //else if (battery_scale == 3)
    {
        if ( vbatt_now <= battery_capacity_up_table_volt_25_ac_400[0])
            return 70;
        else
            return ((vbatt_now - vbatt_scale_min) *10 / (vbatt_scale_max - vbatt_scale_min))  + (index*10) + 70;
    }
    else if (tIndex == up_table_volt_25_usb_new1)    //else if (battery_scale == 2)
    {
        if ( vbatt_now <= battery_capacity_up_table_volt_25_usb_new1[0])
            return 90; //80
        else
            return ((vbatt_now - vbatt_scale_min) *5 / (vbatt_scale_max - vbatt_scale_min))  + (index*5) + (90);// 10 80
    }
    else
    {
        if (vbatt_now > vbatt_scale_max)
            vbatt_now = vbatt_scale_max;

        result = ((vbatt_now - vbatt_scale_min) *5 / (vbatt_scale_max - vbatt_scale_min))  + (index*5);
        if ( (((vbatt_now - vbatt_scale_min) *5) -((result-(index*5))*(vbatt_scale_max - vbatt_scale_min)))>=((vbatt_scale_max - vbatt_scale_min)/2))
            result = result +1;
        if (result<1)
            result = result +1;

        return result;
    }

}

int SelectTable(short tIndex, int battery_scale, int average_adc_vbatt_mv,
                int *battery_capacity_down_table_volt, int *battery_capacity_up_table_volt)
{
    unsigned char    battery_capacity_check_index = 0xFF;
    unsigned char    battery_capacity_check_index_offset = 0;
    int i = 0;
    int adc_battery_capacity = 0;

    battery_capacity_check_index = battery_scale/2 ;//BATTERY_CAPACITY_SCALE / 2;
    for ( i = 1 ; i < battery_scale + 3 ; i = i*2 )//i < BATTERY_CAPACITY_SCALE
    {
        battery_capacity_check_index_offset = ( battery_scale / ( i * 4 ) );//( BATTERY_CAPACITY_SCALE / ( i * 4 ) )

        if ( battery_capacity_check_index_offset == 0 )
            battery_capacity_check_index_offset = 1;

        //printk("[Battery] Check_Index:%d, offset:%d\n",battery_capacity_check_index, battery_capacity_check_index_offset);

        if ( average_adc_vbatt_mv < battery_capacity_down_table_volt[battery_capacity_check_index] )
        {
            battery_capacity_check_index = battery_capacity_check_index - battery_capacity_check_index_offset;
            if (battery_capacity_check_index ==0)
            {
                //if (battery_scale == 9 ||battery_scale == 2 ||battery_scale == 7 ||battery_scale == 3)
                if (tIndex > MAX_DISCHARGE_TABLE)
                {
                    adc_battery_capacity =
                        Scale_To_Capacity(
                            tIndex,
                            battery_capacity_check_index,
                            average_adc_vbatt_mv,
                            battery_capacity_down_table_volt[battery_capacity_check_index],
                            battery_capacity_down_table_volt[battery_capacity_check_index+1]
                        );
                }
                else
                {
                    adc_battery_capacity =
                        Scale_To_Capacity(
                            tIndex,
                            battery_capacity_check_index,
                            average_adc_vbatt_mv,
                            battery_capacity_down_table_volt[battery_capacity_check_index],
                            battery_capacity_down_table_volt[battery_capacity_check_index+21]
                        );
                }
                break;
            }
        }
        else if ( average_adc_vbatt_mv > battery_capacity_down_table_volt[battery_capacity_check_index+1] )
        {
            /*if ( battery_capacity_check_index >= 10 )//bincent battery_capacity_check_index >= 99
            {
            	adc_battery_capacity =100;
            	break;
            }
            else*/
            //{
            battery_capacity_check_index = battery_capacity_check_index + battery_capacity_check_index_offset;
            if ((battery_capacity_check_index == battery_scale -1) && battery_scale == 14)
            {
                //if (battery_scale ==9 ||battery_scale ==2||battery_scale ==7 ||battery_scale ==3)
                adc_battery_capacity =
                    Scale_To_Capacity(
                        tIndex,
                        battery_capacity_check_index,
                        average_adc_vbatt_mv,
                        battery_capacity_down_table_volt[battery_capacity_check_index],
                        battery_capacity_down_table_volt[battery_capacity_check_index+1]
                    );
                //else
                //adc_battery_capacity = Scale_To_Capacity(battery_capacity_check_index,average_adc_vbatt_mv,battery_capacity_down_table_volt[battery_capacity_check_index],battery_capacity_down_table_volt[battery_capacity_check_index+21]);//bincent battery_capacity_check_index + 1
                break;
            }
            //}
        }
        else
        {
            //if (battery_scale == 9 || battery_scale == 2 || battery_scale == 7 || battery_scale == 3)
            if (tIndex > MAX_DISCHARGE_TABLE)
            {
                adc_battery_capacity =
                    Scale_To_Capacity(
                        tIndex,
                        battery_capacity_check_index,
                        average_adc_vbatt_mv,
                        battery_capacity_down_table_volt[battery_capacity_check_index],
                        battery_capacity_down_table_volt[battery_capacity_check_index+1]
                    );
            }
            else
            {
                adc_battery_capacity =
                    Scale_To_Capacity(
                        tIndex,
                        battery_capacity_check_index,
                        average_adc_vbatt_mv,
                        battery_capacity_down_table_volt[battery_capacity_check_index],
                        battery_capacity_down_table_volt[battery_capacity_check_index+21]
                    );//bincent battery_capacity_check_index + 1
            }
            break;
        }
    }

    //Boundery_check
    if (adc_battery_capacity > 100)
        adc_battery_capacity = 100;
    else if (adc_battery_capacity < 0)
        adc_battery_capacity = 0;

    return adc_battery_capacity;
}

void cci_battery_capacity_report(int average_adc_vbatt_mv, int total_current, int *adc_batt_capacity)
{
    short tIndex = 0;
    int battery_scale = 0;
    static int 				*battery_capacity_down_table_volt = NULL;
    static int 				*battery_capacity_up_table_volt = NULL;

    if (adc_batt_capacity == NULL)
    {
        printk("[Battery]: adc_batt_capacity is null \n");
        return;
    }


    //printk("[Battery]: charger_flag:%d, charger_state:%d, g_chgVld:%d, chg_state:%d\n",
    //charger_flag,charger_state,atomic_read(&g_ChgVld),charger_state);

    if ( (charger_flag == CHG_CHARGING && msm_batt_info.current_chg_source == AC_CHG)
            || (charger_flag == CHG_OT && atomic_read(&g_ChgVld) ==  AC_CHG))     //AC Charging

    {
        if (charger_state == CCI_CHG_SET_CC_MODE)
        {
            tIndex = up_table_volt_25_ac_800;    //battery_capacity_up_table_volt_25_ac_800
            battery_scale = 14;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_ac_charger_cc_mode)",average_adc_vbatt_mv);
        }
        else if (charger_state == CCI_CHG_SET_CV_MODE)
        {
            tIndex = up_table_volt_25_ac_400;    //battery_capacity_up_table_volt_25_ac_400
            battery_scale = 3;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_ac_charger_cv_mode)",average_adc_vbatt_mv);
        }
        else
        {
            tIndex = up_table_volt_25_ac_800;    //battery_capacity_up_table_volt_25_ac_800
            battery_scale = 14;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_ac_charger_cc_mode)",average_adc_vbatt_mv);
        }
        //battery_capacity_down_table_volt = battery_capacity_up_table_volt;
        battery_capacity_up_table_volt = pTableIndex[tIndex];
        battery_capacity_down_table_volt = pTableIndex[tIndex];
    }
    else if ((charger_flag == CHG_CHARGING && msm_batt_info.current_chg_source == USB_CHG)
             || (charger_flag == CHG_OT && atomic_read(&g_ChgVld) == USB_CHG))  //USB Charging
    {
        if (charger_state == CCI_CHG_SET_CC_MODE)// || charger_state ==CCI_CHG_SET_IDLE_MODE)
        {
            tIndex = up_table_volt_25_usb_new;    //battery_capacity_up_table_volt_25_usb_new
            battery_scale = 9;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_usb_charger_cc_mode)",average_adc_vbatt_mv);
        }
        else if (charger_state == CCI_CHG_SET_CV_MODE)
        {
            tIndex = up_table_volt_25_usb_new1;    //battery_capacity_up_table_volt_25_usb_new1
            battery_scale = 2;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_usb_charger_cv_mode)",average_adc_vbatt_mv);
        }
        else
        {
            tIndex = up_table_volt_25_usb_new;    //battery_capacity_up_table_volt_25_usb_new
            battery_scale = 9;
            printk("Battery average_adc_vbatt_mv:%d" "(Table_usb_charger_cc_mode)",average_adc_vbatt_mv);
        }
        //battery_capacity_down_table_volt = battery_capacity_up_table_volt;
        battery_capacity_up_table_volt = pTableIndex[tIndex];
        battery_capacity_down_table_volt = pTableIndex[tIndex];
        //}
    }
//    else if (previous_capacity_down_table == NULL)        //[comment] For discharge
    else                                                                     //weijiun_ling, modify
    {
        battery_scale = 20;
        if (total_current >= 800)
        {
            tIndex = down_table_volt_25_800;    //battery_capacity_down_table_volt_25_800_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_800)",average_adc_vbatt_mv);
        }
        else if (total_current >= 700)
        {
            tIndex = down_table_volt_25_700;    //battery_capacity_down_table_volt_25_700_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_700)",average_adc_vbatt_mv);
        }
        else if (total_current >= 650)
        {
            tIndex = down_table_volt_25_650;    //battery_capacity_down_table_volt_25_650_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_650)",average_adc_vbatt_mv);
        }
        else if (total_current >= 600)
        {
            tIndex = down_table_volt_25_600;    //battery_capacity_down_table_volt_25_600_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_600)",average_adc_vbatt_mv);
        }
        else if (total_current >= 550)
        {
            tIndex = down_table_volt_25_550;    //battery_capacity_down_table_volt_25_550_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_550)",average_adc_vbatt_mv);
        }
        else if (total_current >= 500)
        {
            tIndex = down_table_volt_25_500;    //battery_capacity_down_table_volt_25_500_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_500)",average_adc_vbatt_mv);
        }
        else if (total_current >= 450)
        {
            tIndex = down_table_volt_25_450;    //battery_capacity_down_table_volt_25_450_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_450)",average_adc_vbatt_mv);
        }
        else if (total_current >= 400)
        {
            tIndex = down_table_volt_25_400;    //battery_capacity_down_table_volt_25_400_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_400)",average_adc_vbatt_mv);
        }
        else if (total_current >= 350)
        {
            tIndex = down_table_volt_25_350;    //battery_capacity_down_table_volt_25_350_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_350)",average_adc_vbatt_mv);
        }
        else if (total_current >= 300)
        {
            tIndex = down_table_volt_25_300;    //battery_capacity_down_table_volt_25_300_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_300)",average_adc_vbatt_mv);
        }
        else if (total_current >= 250)
        {
            tIndex = down_table_volt_25_250;    //battery_capacity_down_table_volt_25_250_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_250)",average_adc_vbatt_mv);
        }
        else if (total_current >= 200)
        {
            tIndex = down_table_volt_25_200;    //battery_capacity_down_table_volt_25_200_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_200)",average_adc_vbatt_mv);
        }
        else if (total_current >= 150)
        {
            tIndex = down_table_volt_25_150;    //battery_capacity_down_table_volt_25_150_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_150)",average_adc_vbatt_mv);
        }
        else if (total_current >= 100)
        {
            tIndex = down_table_volt_25_100;    //battery_capacity_down_table_volt_25_100_new
            printk("Battery average_adc_vbatt_mv:%d" "(Table_25_100)",average_adc_vbatt_mv);
        }
        battery_capacity_down_table_volt = pTableIndex[tIndex];

    }

    printk("\n");
    printk("[Battery]: Current battery table:%d, bScale:%d, avg_adc:%d\n", tIndex, battery_scale, average_adc_vbatt_mv);
    //if ( adc_battery_capacity == 0xFF )
    //{
    if (call_state == ENABLE_BATT_DBG_INFO)
        printk("Battery scale :%d\n",battery_scale);

    *adc_batt_capacity = SelectTable(tIndex, battery_scale, average_adc_vbatt_mv,
                                     battery_capacity_down_table_volt, battery_capacity_up_table_volt);

    if (msm_batt_info.batt_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
        previous_capacity_down_table = battery_capacity_down_table_volt;


}


static u32 msm_batt_capacity(u32 current_voltage)
{
    int adc_battery_capacity = 0x0;

//printk(KERN_INFO "[Battery]: %s(), current_voltage:%d \n", __func__, current_voltage);


    if (current_voltage < BATTERY_LOW)
    {
        //printk("current_voltage = %d\n",current_voltage);
        if (call_state == PHONE_OFFHOOK && current_voltage > 3300)
            return 1;
        else
            return 0;
    }
    else if ((msm_batt_info.batt_status == POWER_SUPPLY_STATUS_FULL) || charger_flag == CHG_COMPLETE)
        return 100;
    else if (current_voltage >= BATTERY_HIGH)
    {
        if (msm_batt_info.batt_status == POWER_SUPPLY_STATUS_CHARGING || charger_flag == CHG_CHARGING)
        {
            if (msm_batt_info.current_chg_source & AC_CHG)
            {
                if (charger_state == CCI_CHG_SET_CC_MODE)
                    return 70;
                else
                    return 99;
            }
            else if (msm_batt_info.current_chg_source & USB_CHG)
            {
                if (charger_state == CCI_CHG_SET_CC_MODE)
                    return 90;
                else
                    return 99;
            }
            else
                return 99;
        }
        else
            return 99;

    }
    else
    {
        //printk("cci_cal_current = %d ,cci_batt_device_mask = 0x%x\n",cci_cal_current,cci_batt_device_mask);
        cci_battery_capacity_report(current_voltage, cci_cal_current, &adc_battery_capacity);
        return adc_battery_capacity;
    }

}

#ifdef CONFIG_HAS_EARLYSUSPEND
void msm_batt_early_suspend(struct early_suspend *h)
{
    //int rc;

    //msm_batt_lcd_backlight_flag = 1;//bincent
    printk(KERN_INFO "%s(): going to early suspend\n", __func__);


    getnstimeofday(&pre_ts);
    //rtc_time_to_tm(pre_ts.tv_sec, &pre_tm);


    //rc = msm_batt_send_event(SUSPEND_EVENT);

    /*printk(KERN_INFO "%s(): Handled early suspend event."
           " rc = %d\n", __func__, rc);*/
}

void msm_batt_late_resume(struct early_suspend *h)
{

    printk(KERN_INFO "[Battery]: %s(), charger_flag:%d\n", __func__, charger_flag);

    //printk(KERN_INFO "[Battery]: %s(), call msm_batt_update_phy_status_v3 \n", __func__);
    if (is_update_v3 == false)        //[BugID:607]
    msm_batt_update_psy_status_v3();

    is_update_v3 = false;    //[BugID:607]

    Update_Power_Status();
    atomic_set(&g_IsSuspend, false);
    atomic_set(&g_IsOnCall, false);


    //rc = msm_batt_send_event(RESUME_EVENT);

    /*printk(KERN_INFO "%s(): Handled Late resume event."
           " rc = %d\n", __func__, rc);*/
}
#endif

static int batt_suspend(struct platform_device *dev, pm_message_t state)
{

    printk(KERN_INFO "[Battery]: %s()\n", __func__);


// weijiun_ling
//    getnstimeofday(&pre_ts);
//    rtc_time_to_tm(pre_ts.tv_sec, &pre_tm);
    g_Suspend_Chg_Source = msm_batt_info.current_chg_source;

    atomic_set(&g_IsSuspend, true);    //false at late resume

    if (call_state == PHONE_OFFHOOK)
        atomic_set(&g_IsOnCall, true);   //false at late resume

    printk("batt_suspend\n");
    return 0;
}

static int batt_resume(struct platform_device *dev)
{
    cci_smem_value_t *smem_cci_smem_value;
    struct input_dev *kpdev;
    smem_cci_smem_value = smem_alloc( SMEM_CCI_SMEM_VALUE, sizeof( cci_smem_value_t ) );

    printk(KERN_INFO "[Battery]: %s(), charger_flag:%d, smem:%d, ", __func__,charger_flag, smem_cci_smem_value->cci_battery_info);

    if ( smem_cci_smem_value->cci_battery_info == CCI_BATTERY_POWERDOWN)
    {
        printk("Battery Verylow Warning!!!\n");

        kpdev = msm_keypad_get_input_dev();
        input_report_key(kpdev, KEY_POWER, 1);
        input_report_key(kpdev, KEY_POWER, 0);
        smem_cci_smem_value->cci_battery_info = 0;

    }
    else if (smem_cci_smem_value->cci_battery_info == CCI_BATTERY_CRITICAL)
    {
        printk("Battery Critical Warning!!!\n");

        kpdev = msm_keypad_get_input_dev();
        input_report_key(kpdev, KEY_POWER, 1);
        input_report_key(kpdev, KEY_POWER, 0);
        smem_cci_smem_value->cci_battery_info = 0;
    }
    else if (smem_cci_smem_value->cci_modem_charging_state == CCI_CHG_STATE_COMPLETE)
    {
        printk("Battery Full Charger!!!\n");

        if (msm_batt_info.batt_status != POWER_SUPPLY_STATUS_FULL)
        {
            printk("Battery First Full Charger!!!\n");
            kpdev = msm_keypad_get_input_dev();
            input_report_key(kpdev, KEY_POWER, 1);
            input_report_key(kpdev, KEY_POWER, 0);
            smem_cci_smem_value->cci_modem_charging_state  = 0;
        }
    }
    else
    {
        printk("batt_resume\n");
    }

    msm_batt_update_psy_status_v2();
    return 0;
}


static int msm_batt_enable_filter(u32 vbatt_filter)
{
    int rc;
    struct rpc_req_vbatt_filter
    {
        struct rpc_request_hdr hdr;
        u32 batt_handle;
        u32 enable_filter;
        u32 vbatt_filter;
    } req_vbatt_filter;

    struct rpc_rep_vbatt_filter
    {
        struct rpc_reply_hdr hdr;
        u32 filter_enable_result;
    } rep_vbatt_filter;

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    req_vbatt_filter.batt_handle = cpu_to_be32(msm_batt_info.batt_handle);
    req_vbatt_filter.enable_filter = cpu_to_be32(1);
    req_vbatt_filter.vbatt_filter = cpu_to_be32(vbatt_filter);

    rc = msm_rpc_call_reply(msm_batt_info.batt_ep,
                            BATTERY_ENABLE_DISABLE_FILTER_PROC,
                            &req_vbatt_filter, sizeof(req_vbatt_filter),
                            &rep_vbatt_filter, sizeof(rep_vbatt_filter),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR
               "%s: msm_rpc_call_reply failed! proc = %d rc = %d\n",
               __func__, BATTERY_ENABLE_DISABLE_FILTER_PROC, rc);
        return rc;
    }
    else
    {
        rc =  be32_to_cpu(rep_vbatt_filter.filter_enable_result);

        if (rc != BATTERY_DEREGISTRATION_SUCCESSFUL)
        {
            printk(KERN_ERR "%s: vbatt Filter enable failed."
                   " proc = %d  filter enable result = %d"
                   " filter number = %d\n", __func__,
                   BATTERY_ENABLE_DISABLE_FILTER_PROC, rc,
                   vbatt_filter);
            return -EIO;
        }

        printk(KERN_INFO "%s: vbatt Filter enabled successfully.\n",
               __func__);
        return 0;
    }
}


static int __devinit msm_batt_probe(struct platform_device *pdev)
{

    int rc;
    struct msm_psy_batt_pdata *pdata = pdev->dev.platform_data;

//weijiun_ling
    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    if (pdev->id != -1)
    {
        dev_err(&pdev->dev,
                "%s: MSM chipsets Can only support one"
                " battery ", __func__);
        return -EINVAL;
    }

    if (pdata->avail_chg_sources & AC_CHG)
    {
        rc = power_supply_register(&pdev->dev, &msm_psy_ac);
        if (rc < 0)
        {
            dev_err(&pdev->dev,
                    "%s: power_supply_register failed"
                    " rc = %d\n", __func__, rc);
            msm_batt_cleanup();
            return rc;
        }
        msm_batt_info.msm_psy_ac = &msm_psy_ac;
        msm_batt_info.avail_chg_sources |= AC_CHG;
    }

    if (pdata->avail_chg_sources & USB_CHG)
    {
        rc = power_supply_register(&pdev->dev, &msm_psy_usb);
        if (rc < 0)
        {
            dev_err(&pdev->dev,
                    "%s: power_supply_register failed"
                    " rc = %d\n", __func__, rc);
            msm_batt_cleanup();
            return rc;
        }
        msm_batt_info.msm_psy_usb = &msm_psy_usb;
        msm_batt_info.avail_chg_sources |= USB_CHG;
    }

    if (!msm_batt_info.msm_psy_ac && !msm_batt_info.msm_psy_usb)
    {

        dev_err(&pdev->dev,
                "%s: No external Power supply(AC or USB)"
                "is avilable\n", __func__);
        msm_batt_cleanup();
        return -ENODEV;
    }

    msm_batt_info.voltage_max_design = pdata->voltage_max_design;
    msm_batt_info.voltage_min_design = pdata->voltage_min_design;
    msm_batt_info.batt_technology = pdata->batt_technology;
    //msm_batt_info.calculate_capacity = pdata->calculate_capacity;

    if (!msm_batt_info.voltage_min_design)
        msm_batt_info.voltage_min_design = BATTERY_LOW;
    if (!msm_batt_info.voltage_max_design)
        msm_batt_info.voltage_max_design = BATTERY_HIGH;

    if (msm_batt_info.batt_technology == POWER_SUPPLY_TECHNOLOGY_UNKNOWN)
        msm_batt_info.batt_technology = POWER_SUPPLY_TECHNOLOGY_LION;
    //bincent add
    if (msm_batt_info.batt_status == POWER_SUPPLY_STATUS_UNKNOWN)
        msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;

    if (!msm_batt_info.calculate_capacity)
        msm_batt_info.calculate_capacity = msm_batt_capacity;

    rc = power_supply_register(&pdev->dev, &msm_psy_batt);
    if (rc < 0)
    {
        dev_err(&pdev->dev, "%s: power_supply_register failed"
                " rc=%d\n", __func__, rc);
        msm_batt_cleanup();
        return rc;
    }
    msm_batt_info.msm_psy_batt = &msm_psy_batt;

    rc = msm_batt_register(BATTERY_LOW, BATTERY_ALL_ACTIVITY,
                           BATTERY_CB_ID_ALL_ACTIV, BATTERY_ALL_ACTIVITY);
    if (rc < 0)
    {
        dev_err(&pdev->dev,
                "%s: msm_batt_register failed rc=%d\n", __func__, rc);
        msm_batt_cleanup();
        return rc;
    }
    msm_batt_info.batt_handle = rc;

    rc =  msm_batt_enable_filter(VBATT_FILTER);

#ifdef CONFIG_HAS_EARLYSUSPEND
    msm_batt_info.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    msm_batt_info.early_suspend.suspend = msm_batt_early_suspend;
    msm_batt_info.early_suspend.resume = msm_batt_late_resume;
    register_early_suspend(&msm_batt_info.early_suspend);
#endif
    wake_lock_init(&msm_batt_info.wlock, WAKE_LOCK_SUSPEND, "low_battery_shutdown");//bincent



    INIT_DELAYED_WORK(&msm_batt_info.dwork, msm_batt_wait_for_batt_chg_event);
    msm_batt_start_cb_thread();

    return 0;
}


int msm_batt_get_charger_api_version(void)
{
    int rc ;
    struct rpc_reply_hdr *reply;

    struct rpc_req_chg_api_ver
    {
        struct rpc_request_hdr hdr;
        u32 more_data;
    } req_chg_api_ver;

    struct rpc_rep_chg_api_ver
    {
        struct rpc_reply_hdr hdr;
        u32 num_of_chg_api_versions;
        u32 *chg_api_versions;
    };

    u32 num_of_versions;

    struct rpc_rep_chg_api_ver *rep_chg_api_ver;


    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    req_chg_api_ver.more_data = cpu_to_be32(1);

    msm_rpc_setup_req(&req_chg_api_ver.hdr, CHG_RPC_PROG, CHG_RPC_VERS,
                      ONCRPC_CHARGER_API_VERSIONS_PROC);

    rc = msm_rpc_write(msm_batt_info.chg_ep, &req_chg_api_ver,
                       sizeof(req_chg_api_ver));
    if (rc < 0)
    {
        printk(KERN_ERR
               "%s(): msm_rpc_write failed.  proc = 0x%08x rc = %d\n",
               __func__, ONCRPC_CHARGER_API_VERSIONS_PROC, rc);
        return rc;
    }

    for (;;)
    {
        rc = msm_rpc_read(msm_batt_info.chg_ep, (void *) &reply, -1,
                          BATT_RPC_TIMEOUT);
        if (rc < 0)
            return rc;
        if (rc < RPC_REQ_REPLY_COMMON_HEADER_SIZE)
        {
            printk(KERN_ERR "%s(): msm_rpc_read failed. read"
                   " returned invalid packet which is"
                   " neither rpc req nor rpc reply. "
                   "legnth of packet = %d\n",
                   __func__, rc);

            rc = -EIO;
            break;
        }
        /* we should not get RPC REQ or call packets -- ignore them */
        if (reply->type == RPC_TYPE_REQ)
        {

            printk(KERN_ERR "%s(): returned RPC REQ packets while"
                   " waiting for RPC REPLY replay read \n",
                   __func__);
            kfree(reply);
            continue;
        }

        /* If an earlier call timed out, we could get the (no
         * longer wanted) reply for it.	 Ignore replies that
         * we don't expect
         */
        if (reply->xid != req_chg_api_ver.hdr.xid)
        {

            printk(KERN_ERR "%s(): returned RPC REPLY XID is not"
                   " equall to RPC REQ XID \n", __func__);
            kfree(reply);
            continue;
        }
        if (reply->reply_stat != RPCMSG_REPLYSTAT_ACCEPTED)
        {
            rc = -EPERM;
            break;
        }
        if (reply->data.acc_hdr.accept_stat !=
                RPC_ACCEPTSTAT_SUCCESS)
        {
            rc = -EINVAL;
            break;
        }

        rep_chg_api_ver = (struct rpc_rep_chg_api_ver *)reply;

        num_of_versions =
            be32_to_cpu(rep_chg_api_ver->num_of_chg_api_versions);

        rep_chg_api_ver->chg_api_versions =  (u32 *)
                                             ((u8 *) reply + sizeof(struct rpc_reply_hdr) +
                                              sizeof(rep_chg_api_ver->num_of_chg_api_versions));

        rc = be32_to_cpu(
                 rep_chg_api_ver->chg_api_versions[num_of_versions - 1]);

        printk(KERN_INFO "%s(): num_of_chg_api_versions = %u"
               "  The chg api version = 0x%08x\n", __func__,
               num_of_versions, rc);
        break;
    }
    kfree(reply);
    return rc;
}


static struct platform_driver msm_batt_driver;
static int __devinit msm_batt_init_rpc(void)
{
    int rc;

    spin_lock_init(&msm_batt_info.lock);

    msm_batt_info.msm_batt_wq =
        create_singlethread_workqueue("msm_battery");

    if (!msm_batt_info.msm_batt_wq)
    {
        printk(KERN_ERR "%s: create workque failed \n", __func__);
        return -ENOMEM;
    }

    msm_batt_info.batt_ep =
        msm_rpc_connect_compatible(BATTERY_RPC_PROG, BATTERY_RPC_VERS, 0);

    if (msm_batt_info.batt_ep == NULL)
    {
        return -ENODEV;
    }
    else if (IS_ERR(msm_batt_info.batt_ep))
    {
        int rc = PTR_ERR(msm_batt_info.batt_ep);
        printk(KERN_ERR
               "%s: rpc connect failed for BATTERY_RPC_PROG."
               " rc = %d\n ", __func__, rc);
        msm_batt_info.batt_ep = NULL;
        return rc;
    }

    msm_batt_info.chg_ep =
        msm_rpc_connect_compatible(CHG_RPC_PROG, CHG_RPC_VERS, 0);

    if (msm_batt_info.chg_ep == NULL)
    {
        return -ENODEV;
    }
    else if (IS_ERR(msm_batt_info.chg_ep))
    {
        int rc = PTR_ERR(msm_batt_info.chg_ep);
        printk(KERN_ERR
               "%s: rpc connect failed for CHG_RPC_PROG. rc = %d\n",
               __func__, rc);
        msm_batt_info.chg_ep = NULL;
        return rc;
    }

    //bincent cci_ep
    msm_batt_info.cci_ep = msm_rpc_connect(CCIPROG, CCIVERS, 0);
    if (msm_batt_info.cci_ep == NULL)
    {
        return -ENODEV;
    }
    else if (IS_ERR(msm_batt_info.cci_ep))
    {
        int rc = PTR_ERR(msm_batt_info.cci_ep);
        printk(KERN_ERR
               "%s: rpc connect failed for CCI_RPC_PROG. rc = %d\n",
               __func__, rc);
        msm_batt_info.cci_ep = NULL;
        return rc;
    }
    //end

    msm_batt_info.chg_api_version =  msm_batt_get_charger_api_version();

    //if (msm_batt_info.chg_api_version < 0)
    msm_batt_info.chg_api_version = DEFAULT_CHARGER_API_VERSION;

    rc = platform_driver_register(&msm_batt_driver);

    if (rc < 0)
    {
        printk(KERN_ERR "%s: platform_driver_register failed for "
               "batt driver. rc = %d\n", __func__, rc);
        return rc;
    }

    init_waitqueue_head(&msm_batt_info.wait_q);

    return 0;
}

static int __devexit msm_batt_remove(struct platform_device *pdev)
{
    int rc;
    wake_lock_destroy(&msm_batt_info.wlock);
    rc = msm_batt_cleanup();

    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    if (rc < 0)
    {
        dev_err(&pdev->dev,
                "%s: msm_batt_cleanup  failed rc=%d\n", __func__, rc);
        return rc;
    }
    return 0;
}

//[Bug 807] Add battery log on framework layer, weijiun_ling, 2010.10.13
ssize_t  batt_read(struct file *filp, char __user *buffer, size_t size, loff_t *offset)
{
    int err;
#ifdef BATTERY_LOGCAT
    err = copy_to_user(buffer, &dbglog, min(sizeof(dbglog), size));
    return err ? -EFAULT : 0;
#else
    return 0;
#endif
}
//End

static struct file_operations batt_fops = {
	.owner = THIS_MODULE,
	.read = batt_read,

};

static struct miscdevice batt_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "misc_batt",
	.fops = &batt_fops,
};

//END

static struct platform_driver msm_batt_driver =
{
    .probe = msm_batt_probe,
    .suspend = batt_suspend,
    .resume	= batt_resume,
    .remove = __devexit_p(msm_batt_remove),
    .driver = {
        .name = "msm-battery",
        .owner = THIS_MODULE,
    },
};

static int call_state_proc_write(struct file *file, const char *buffer,
                                 unsigned long count, void *data)
{
    char *buf;
    int BUFIndex;

    if (count < 1)
        return -EINVAL;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count))
    {
        kfree(buf);
        return -EFAULT;
    }

    BUFIndex = (int)simple_strtol(buf, NULL, 10);


    printk(KERN_INFO "[Battery]: %s(), BUFIndex:%d, buffer:%s \n", __func__, BUFIndex,buffer);

    switch (BUFIndex)
    {
        case PHONE_IDLE:
            printk("Phone idle\n");
            call_state = PHONE_IDLE;
            break;

        case PHONE_RINGING:
            printk("Phone Ringing\n");
            call_state = PHONE_RINGING;
            break;

        case PHONE_OFFHOOK:
            printk("Phone OffHook\n");
            call_state = PHONE_OFFHOOK;
            break;

        case ENABLE_BATT_DBG_INFO:
            printk("Enable battery debug info\n");
            call_state = ENABLE_BATT_DBG_INFO;
            break;

        case ENABLE_CHARGING_FUNC:
            printk("Enable Charging function\n");
            call_state =  ENABLE_CHARGING_FUNC;		

            cancel_delayed_work_sync(&msm_batt_info.dwork);
            schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(1*1000));
            break;

        case DISABLE_CHARGING_FUNC:
            printk("Disable Charging function\n");
            call_state = DISABLE_CHARGING_FUNC;

            cancel_delayed_work_sync(&msm_batt_info.dwork);
            schedule_delayed_work(&msm_batt_info.dwork, msecs_to_jiffies(1*1000));
            break;


    }

    kfree(buf);
    return count;

}

//[Bug 919] To modify getting the initialize voltage at bootup, weijiun_ling, 2010.10.28
int Get_init_voltage(s32* val)
{
    int rc;
    s32 value;

    struct rpc_request_hdr req_batt_chg;

    struct rpc_reply_chg_reply
    {
        struct rpc_reply_hdr hdr;
        s32 chg_batt_data;
    } rep_chg;

    rc = msm_rpc_call_reply(msm_batt_info.cci_ep,
                            ONCRPC_CCI_GET_VBAT_BEFORE_OS_RUN,
                            &req_batt_chg, sizeof(req_batt_chg),
                            &rep_chg, sizeof(rep_chg),
                            msecs_to_jiffies(BATT_RPC_TIMEOUT));
    if (rc < 0)
    {
        printk(KERN_ERR "[Battery]: %s: msm_rpc_call_reply failed! proc=%d rc=%d\n",
               __func__, ONCRPC_CCI_GET_VBAT_BEFORE_OS_RUN, rc);
        return rc;
    }

    value = be32_to_cpu(rep_chg.chg_batt_data);
    *val = value;
    printk("[Battery]: %s, voltage:%d\n", __func__, value);

    return 0;
}
//End

static void Batt_init_proc(struct work_struct *work)
{
    u32 batt_charging = 0;
    u32 batt_volt = 0;
    u32 chg_batt_event = CHG_UI_EVENT__INVALID;
    s32 chg_batt_temp = 0;//over temp
    s32 power_level_dbm = 0;
    u32 charger_valid = 0;
    s32 localpercent;
    s32 amss_chg_state = 0;

    msm_batt_get_batt_rpc(&batt_volt,&batt_charging, &charger_valid,
                          &chg_batt_event,&chg_batt_temp,&power_level_dbm,&amss_chg_state);

    //[BugID:505] [BSP][BATT] To fix the problem of getting first battery info
    //To switch charging table if plugging charger at initialize
    if (charger_state > CCI_CHG_SET_IDLE_MODE && charger_state < CCI_CHG_SET_MODE_INVALID)
    {
        //[Bug 1008], To modify getting the initialize voltage at bootup (Including AC & USB), weijiun_ling, 2010.11.12
        charger_flag = CHG_NONE;
        msm_batt_info.batt_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
        msm_batt_info.current_chg_source = NO_CHG;

        Get_init_voltage(&batt_volt);
        localpercent = msm_batt_capacity(batt_volt);

        //End
        charger_flag = CHG_CHARGING;
        msm_batt_info.batt_status = POWER_SUPPLY_STATUS_CHARGING;
        msm_batt_info.current_chg_source = AC_CHG;
    }
    //[Bug 919] To modify getting the initialize voltage at bootup, weijiun_ling, 2010.10.28
    else if (charger_state == CCI_CHG_SET_IDLE_MODE)
    {
        Get_init_voltage(&batt_volt);
        cci_cal_current = 100;
        localpercent = msm_batt_capacity(batt_volt);
    }
    //End

    T_Capacity_uAH = CONV_PERCENT_TO_uAH(localpercent);
    msm_batt_info.batt_capacity = localpercent;

    printk("[Battery]: Batt_init_proc, LF:%d, V:%d, T:%d, M_Chgr:%d, Flg:%d\n",
               localpercent, batt_volt, chg_batt_temp, charger_state, charger_flag);

}


static int __init msm_batt_init(void)
{
    int rc;
    struct proc_dir_entry *d_entry;

    //printk("msm_batt_init\n");
    printk(KERN_INFO "[Battery]: %s()\n", __func__);

    rc = msm_batt_init_rpc();

    if (rc < 0)
    {
        printk(KERN_ERR "%s: msm_batt_init_rpc Failed  rc=%d\n",
               __func__, rc);
        msm_batt_cleanup();
        return rc;
    }

    //weijiun_ling
    INIT_DELAYED_WORK(&g_dwork, Batt_init_proc);

    //[BugID:505] [BSP][BATT] To fix the problem of getting first battery info
    schedule_delayed_work(&g_dwork, msecs_to_jiffies(10*1000));

//[Bug 807] Add battery log on framework layer, weijiun_ling, 2010.10.13
    misc_register(&batt_device);
//End

    d_entry = create_proc_entry("call_state",
                                S_IRWXU | S_IRWXO, NULL);
    if (d_entry)
    {
        d_entry->read_proc = NULL;
        d_entry->write_proc = call_state_proc_write;
        d_entry->data = NULL;
    }
    else
    {
        printk("Unable to create /proc/call_state entry\n");
        remove_proc_entry("call_state", 0);
        return  -ENOMEM;
    }


    return 0;
}

static void __exit msm_batt_exit(void)
{
    platform_driver_unregister(&msm_batt_driver);
}

module_init(msm_batt_init);
module_exit(msm_batt_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Kiran Kandi, Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("Battery driver for Qualcomm MSM chipsets.");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:msm_battery");

