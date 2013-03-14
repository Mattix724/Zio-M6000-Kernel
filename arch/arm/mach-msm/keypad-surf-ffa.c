/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <asm/mach-types.h>
#include <linux/wakelock.h>

/* don't turn this on without updating the ffa support */
#define SCAN_FUNCTION_KEYS 0

/* FFA:
 36: KEYSENSE_N(0)
 37: KEYSENSE_N(1)
 38: KEYSENSE_N(2)
 39: KEYSENSE_N(3)
 40: KEYSENSE_N(4)

 31: KYPD_17
 32: KYPD_15
 33: KYPD_13
 34: KYPD_11
 35: KYPD_9
 41: KYPD_MEMO
*/

static unsigned int keypad_row_gpios[] = {
	31, 32, 33, 34, 35, 41
#if SCAN_FUNCTION_KEYS
	, 42
#endif
};

static unsigned int keypad_col_gpios[] = { 36, 37, 38, 39, 40 };

static unsigned int keypad_row_gpios_8k_ffa[] = {31, 32, 33, 34, 35, 36};
static unsigned int keypad_col_gpios_8k_ffa[] = {38, 39, 40, 41, 42};

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios) + (col))
#define FFA_8K_KEYMAP_INDEX(row, col) ((row)* \
				ARRAY_SIZE(keypad_col_gpios_8k_ffa) + (col))

static const unsigned short keypad_keymap_surf[ARRAY_SIZE(keypad_col_gpios) *
					  ARRAY_SIZE(keypad_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_5,
	[KEYMAP_INDEX(0, 1)] = KEY_9,
	[KEYMAP_INDEX(0, 2)] = 229,            /* SOFT1 */
	[KEYMAP_INDEX(0, 3)] = KEY_6,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_0,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_1,
	[KEYMAP_INDEX(1, 3)] = 228,           /* KEY_SHARP */
	[KEYMAP_INDEX(1, 4)] = KEY_SEND,

	[KEYMAP_INDEX(2, 0)] = KEY_VOLUMEUP,
	[KEYMAP_INDEX(2, 1)] = KEY_HOME,      /* FA   */
	[KEYMAP_INDEX(2, 2)] = KEY_F8,        /* QCHT */
	[KEYMAP_INDEX(2, 3)] = KEY_F6,        /* R+   */
	[KEYMAP_INDEX(2, 4)] = KEY_F7,        /* R-   */

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = KEY_CLEAR,
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	[KEYMAP_INDEX(3, 3)] = KEY_MUTE,      /* SPKR */
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = 230,           /* SOFT2 */
	[KEYMAP_INDEX(4, 1)] = 232,           /* KEY_CENTER */
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_BACK,      /* FB */
	[KEYMAP_INDEX(4, 4)] = KEY_8,

	[KEYMAP_INDEX(5, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[KEYMAP_INDEX(5, 2)] = KEY_MAIL,      /* MESG */
	[KEYMAP_INDEX(5, 3)] = KEY_3,
	[KEYMAP_INDEX(5, 4)] = KEY_7,

#if SCAN_FUNCTION_KEYS
	[KEYMAP_INDEX(6, 0)] = KEY_F5,
	[KEYMAP_INDEX(6, 1)] = KEY_F4,
	[KEYMAP_INDEX(6, 2)] = KEY_F3,
	[KEYMAP_INDEX(6, 3)] = KEY_F2,
	[KEYMAP_INDEX(6, 4)] = KEY_F1
#endif
};

static const unsigned short keypad_keymap_ffa[ARRAY_SIZE(keypad_col_gpios) *
					      ARRAY_SIZE(keypad_row_gpios)] = {
	/*[KEYMAP_INDEX(0, 0)] = ,*/
	/*[KEYMAP_INDEX(0, 1)] = ,*/
	[KEYMAP_INDEX(0, 2)] = KEY_1,
	[KEYMAP_INDEX(0, 3)] = KEY_SEND,
	[KEYMAP_INDEX(0, 4)] = KEY_LEFT,

	[KEYMAP_INDEX(1, 0)] = KEY_3,
	[KEYMAP_INDEX(1, 1)] = KEY_RIGHT,
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEUP,
	/*[KEYMAP_INDEX(1, 3)] = ,*/
	[KEYMAP_INDEX(1, 4)] = KEY_6,

	[KEYMAP_INDEX(2, 0)] = KEY_HOME,      /* A */
	[KEYMAP_INDEX(2, 1)] = KEY_BACK,      /* B */
	[KEYMAP_INDEX(2, 2)] = KEY_0,
	[KEYMAP_INDEX(2, 3)] = 228,           /* KEY_SHARP */
	[KEYMAP_INDEX(2, 4)] = KEY_9,

	[KEYMAP_INDEX(3, 0)] = KEY_UP,
	[KEYMAP_INDEX(3, 1)] = 232, /* KEY_CENTER */ /* i */
	[KEYMAP_INDEX(3, 2)] = KEY_4,
	/*[KEYMAP_INDEX(3, 3)] = ,*/
	[KEYMAP_INDEX(3, 4)] = KEY_2,

	[KEYMAP_INDEX(4, 0)] = KEY_VOLUMEDOWN,
	[KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[KEYMAP_INDEX(4, 2)] = KEY_DOWN,
	[KEYMAP_INDEX(4, 3)] = KEY_8,
	[KEYMAP_INDEX(4, 4)] = KEY_5,

	/*[KEYMAP_INDEX(5, 0)] = ,*/
	[KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[KEYMAP_INDEX(5, 2)] = 230, /*SOFT2*/ /* 2 */
	[KEYMAP_INDEX(5, 3)] = KEY_MENU,      /* 1 */
	[KEYMAP_INDEX(5, 4)] = KEY_7,
};

#define QSD8x50_FFA_KEYMAP_SIZE (ARRAY_SIZE(keypad_col_gpios_8k_ffa) * \
			ARRAY_SIZE(keypad_row_gpios_8k_ffa))

static const unsigned short keypad_keymap_8k_ffa[QSD8x50_FFA_KEYMAP_SIZE] = {

	[FFA_8K_KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,
	/*[KEYMAP_INDEX(0, 1)] = ,*/
	[FFA_8K_KEYMAP_INDEX(0, 2)] = KEY_DOWN,
	[FFA_8K_KEYMAP_INDEX(0, 3)] = KEY_8,
	[FFA_8K_KEYMAP_INDEX(0, 4)] = KEY_5,

	[FFA_8K_KEYMAP_INDEX(1, 0)] = KEY_UP,
	[FFA_8K_KEYMAP_INDEX(1, 1)] = KEY_CLEAR,
	[FFA_8K_KEYMAP_INDEX(1, 2)] = KEY_4,
	/*[KEYMAP_INDEX(1, 3)] = ,*/
	[FFA_8K_KEYMAP_INDEX(1, 4)] = KEY_2,

	[FFA_8K_KEYMAP_INDEX(2, 0)] = KEY_HOME,      /* A */
	[FFA_8K_KEYMAP_INDEX(2, 1)] = KEY_BACK,      /* B */
	[FFA_8K_KEYMAP_INDEX(2, 2)] = KEY_0,
	[FFA_8K_KEYMAP_INDEX(2, 3)] = 228,           /* KEY_SHARP */
	[FFA_8K_KEYMAP_INDEX(2, 4)] = KEY_9,

	[FFA_8K_KEYMAP_INDEX(3, 0)] = KEY_3,
	[FFA_8K_KEYMAP_INDEX(3, 1)] = KEY_RIGHT,
	[FFA_8K_KEYMAP_INDEX(3, 2)] = KEY_VOLUMEUP,
	/*[KEYMAP_INDEX(3, 3)] = ,*/
	[FFA_8K_KEYMAP_INDEX(3, 4)] = KEY_6,

	[FFA_8K_KEYMAP_INDEX(4, 0)] = 232,		/* OK */
	[FFA_8K_KEYMAP_INDEX(4, 1)] = KEY_SOUND,
	[FFA_8K_KEYMAP_INDEX(4, 2)] = KEY_1,
	[FFA_8K_KEYMAP_INDEX(4, 3)] = KEY_SEND,
	[FFA_8K_KEYMAP_INDEX(4, 4)] = KEY_LEFT,

	/*[KEYMAP_INDEX(5, 0)] = ,*/
	[FFA_8K_KEYMAP_INDEX(5, 1)] = 227,           /* KEY_STAR */
	[FFA_8K_KEYMAP_INDEX(5, 2)] = 230, /*SOFT2*/ /* 2 */
	[FFA_8K_KEYMAP_INDEX(5, 3)] = KEY_MENU,      /* 1 */
	[FFA_8K_KEYMAP_INDEX(5, 4)] = KEY_7,
};

/* cci,johnny_lee, for direct key */
static struct gpio_event_input_info keypad_direct_info;

/* SURF keypad platform device information */
#if 0 /* cci,johnny_lee, for direct key, no used */
static struct gpio_event_matrix_info surf_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_surf,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};
#endif
/* cci,johnny_lee, for direct key, start */
static const unsigned short keypad_virtual_keys[] = {
	KEY_MEDIA, /* cci.johnny_lee, headset button */
	KEY_END,
	KEY_POWER,
	KEY_HOME,
	KEY_SEARCH,
	KEY_MENU,
	KEY_BACK,
	KEY_F4, //KEY_END
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT
};

#define KB60_KEY_TALK                   KEY_SEND
#define KB60_KEY_VOLUMEUP               KEY_VOLUMEUP
#define KB60_KEY_VOLUMEDOWN             KEY_VOLUMEDOWN
#define KB60_KEY_CAM_AF	                KEY_HP
#define KB60_KEY_CAM_CAPTURE            KEY_CAMERA
#define KB60_KEY_ENTER                  KEY_ENTER

static struct gpio_event_direct_entry kb60_direct_keys[] = {
	{41,KB60_KEY_VOLUMEUP },
	{40,KB60_KEY_VOLUMEDOWN },
	{39,KB60_KEY_CAM_AF },
	{38,KB60_KEY_CAM_CAPTURE },
	{37,KB60_KEY_TALK },
	{50,KB60_KEY_ENTER },
};

static int kb60_key_pressed[] = {0,0,0,0,0,0};
static int has_wakelock = 0;
struct wake_lock wake_lock_idle;
struct wake_lock wake_lock_suspend;

static struct gpio_event_info *surf_keypad_info[] = {
#if 0
	&surf_keypad_matrix_info.info
#else
	&keypad_direct_info.info
#endif
};
/* cci,johnny_lee, for direct key, end */

static struct gpio_event_platform_data surf_keypad_data = {
	.name		= "surf_keypad",
	.info		= surf_keypad_info,
	.info_count	= ARRAY_SIZE(surf_keypad_info)
};

struct platform_device keypad_device_surf = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &surf_keypad_data,
	},
};

/* cci,johnny_lee, for direct key, start */
static struct gpio_event_input_info keypad_direct_info = {
	.info.func	= gpio_event_input_func, /* Using gpio_event_input_func instead, james@cci changed for power key (power management), arch/arm/mach-msm/rpc_server_end_key.c */
	.debounce_time.tv = { /* james@cci , this value is just guess to 10 ns */
		.sec = 0,
		.nsec = 1000000
	},
	.flags	    =	0,
	.type = EV_KEY,
	.keymap = kb60_direct_keys,
	.keymap_size = ARRAY_SIZE(kb60_direct_keys)
};
/* cci,johnny_lee, for direct key, end */

#if 0
/* 8k FFA keypad platform device information */
static struct gpio_event_matrix_info keypad_matrix_info_8k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_8k_ffa,
	.output_gpios	= keypad_row_gpios_8k_ffa,
	.input_gpios	= keypad_col_gpios_8k_ffa,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_8k_ffa),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_8k_ffa),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_8k_ffa[] = {
	&keypad_matrix_info_8k_ffa.info
};

static struct gpio_event_platform_data keypad_data_8k_ffa = {
	.name		= "8k_ffa_keypad",
	.info		= keypad_info_8k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_8k_ffa)
};

struct platform_device keypad_device_8k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_8k_ffa,
	},
};
#endif

/* 7k FFA keypad platform device information */
static struct gpio_event_matrix_info keypad_matrix_info_7k_ffa = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_ffa,
	.output_gpios	= keypad_row_gpios,
	.input_gpios	= keypad_col_gpios,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_DRIVE_INACTIVE |
			  GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_info *keypad_info_7k_ffa[] = {
	&keypad_matrix_info_7k_ffa.info
};

static struct gpio_event_platform_data keypad_data_7k_ffa = {
	.name		= "7k_ffa_keypad",
	.info		= keypad_info_7k_ffa,
	.info_count	= ARRAY_SIZE(keypad_info_7k_ffa)
};

struct platform_device keypad_device_7k_ffa = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &keypad_data_7k_ffa,
	},
};

//static struct input_dev *keypad_dev;

int init_surf_ffa_keypad(int ffa)
{
	printk("Keypad Initiation\n");
	return platform_device_register(&keypad_device_surf);
}
struct input_dev *msm_7k_handset_get_input_dev(void);

struct input_dev *msm_keypad_get_input_dev(void)
{
	return msm_7k_handset_get_input_dev();
}

void cci_key_init(void)
{
#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
	extern int HW_ID4;
	if (HW_ID4 == 0) {
		kb60_direct_keys[5].gpio = 50;
	} else {
		kb60_direct_keys[5].gpio = 21; // for Sprint GPIO
}
#else
	kb60_direct_keys[5].gpio = 50;
#endif
	wake_lock_init(&wake_lock_suspend, WAKE_LOCK_SUSPEND, "cci_key_wakelock");
	wake_lock_init(&wake_lock_idle, WAKE_LOCK_IDLE, "cci_key_wakelock_idle");
	return;
}

// for QXDM message service
#ifdef CONFIG_CCI_KEYPAD_QXDM
void keypad_qxdm_send_code(int keycode);
#endif

void cci_key_handler(int keycode, int pressed)
{
	char qxdm_keycode = 0;
	int i;
	int need_wakelock;

	for (i=0, need_wakelock=0;i<keypad_direct_info.keymap_size;i++) {
		if (keycode == keypad_direct_info.keymap[i].code) {
			kb60_key_pressed[i] = (pressed==1) ? 1 : 0;
		}
		if (kb60_key_pressed[i] == 1) {
			need_wakelock = 1;
		}
	}
	//printk(KERN_INFO "%s---------%d,%d,%d,%d,%d,%d\n",__func__,
	//	kb60_key_pressed[0],kb60_key_pressed[1],kb60_key_pressed[2],
	//	kb60_key_pressed[3],kb60_key_pressed[4],kb60_key_pressed[5]);
	if (need_wakelock == 1 && has_wakelock == 0) {
		wake_lock(&wake_lock_suspend);
		wake_lock(&wake_lock_idle);
		has_wakelock = 1;
		//printk(KERN_INFO "%s: IDLE wake_lock\n",__func__);
	} else if (need_wakelock == 0 && has_wakelock == 1) {
		wake_unlock(&wake_lock_idle);
		wake_unlock(&wake_lock_suspend);
		has_wakelock = 0;
		//printk(KERN_INFO "%s: IDLE wake_unlock\n",__func__);
	}

	switch (keycode){
		case KEY_ENTER:
			qxdm_keycode = 0x0C;
		break;
		case KEY_SEND:
			qxdm_keycode = 0x50;
		break;
		case KEY_END:
			qxdm_keycode = 0x51;
		break;
		case KEY_VOLUMEUP:
			qxdm_keycode = 0x8F;
		break;
		case KEY_VOLUMEDOWN:
			qxdm_keycode = 0x90;
		break;
		case KEY_CAMERA:
			qxdm_keycode = 0xA2;
		break;
		default:
		break;
	}
#ifdef CONFIG_CCI_KEYPAD_QXDM
	if (qxdm_keycode != 0 && pressed == 1){
		keypad_qxdm_send_code(qxdm_keycode);
	}
#endif
	printk(KERN_INFO "Key pressed = %d %s\n", keycode, (pressed==1)?" ":" released");
}
