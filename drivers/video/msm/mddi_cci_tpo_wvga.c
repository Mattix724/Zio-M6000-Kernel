/*
 * COMPALCOMM.INC, Author: albert_wu@compalcomm.com
 * copy from drivers/video/msm/src/panel/mddi/mddi_sharp.c
 *
 * Copyright (c) 2008 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"
#include <asm/gpio.h>
#include <mach/msm_rpcrouter.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <linux/proc_fs.h>
#include "../../cci/leds/kb60_leds.h"


#define CCI_KB60_WVGA_PRIM 1
#define LCD_DRV_EN 32
//#define LCM_PWM 122
#define LCD_RST_N 47
//#define USE_CABC_LABC 1

#define CHECK_HWID					1
#define CCIPROG1					0x30001000
#define CCIVERS1					0x10001
#define CCI_READ_MPP_DIGITAL_INPUT	33
#define MPP_14						53
#define MPP_15						54
#define MPP_16						55
#define MPP_17						56

#define MPP_21						20
#define MPP_22						21

int DVT_1_3 = 0;
int DVT_2 = 0;
int PVT = 0;
int HW_ID4 = 0;
int HWVERSION = 0;
int BANDID = 0;

extern uint32 mddi_host_core_version;
//static boolean mddi_cci_kb60_monitor_refresh_value = TRUE;
//static boolean mddi_cci_kb60_report_refresh_measurements = FALSE;
static uint32 mddi_cci_kb60_rows_per_second = 13830;	// 5200000/376
static uint32 mddi_cci_kb60_rows_per_refresh = 338;
static uint32 mddi_cci_kb60_usecs_per_refresh = 24440;	// (376+338)/5200000
static boolean mddi_cci_kb60_debug_60hz_refresh = FALSE;

/* add these variables to prevent suspend white screen, by ivan*/
static boolean mddi_cci_kb60_not_allow_off = FALSE;
//static boolean mddi_cci_kb60_lcd_is_off = FALSE; //james@cci
static boolean mddi_cci_kb60_backlight_is_off = TRUE;



extern mddi_gpio_info_type mddi_gpio;
extern boolean mddi_vsync_detect_enabled;

//static msm_fb_vsync_handler_type mddi_cci_kb60_vsync_handler = NULL;
//static void *mddi_cci_kb60_vsync_handler_arg;
//static uint16 mddi_cci_kb60_vsync_attempts;

static void mddi_cci_kb60_prim_lcd_init(void);
static void mddi_cci_kb60_lcd_set_backlight(struct msm_fb_data_type *mfd);
//static void mddi_cci_kb60_vsync_set_handler(msm_fb_vsync_handler_type handler,void *);
//static void mddi_cci_kb60_lcd_vsync_detected(boolean detected);
static struct msm_panel_common_pdata *mddi_cci_kb60_pdata;
int backlight_val = 128;
int initial = 0;
// Kevin_Chiang@CCI
#define ENABLE_DEBUG 0

#if ENABLE_DEBUG
	#define DEBUG(format,args...) do { \
				printk(KERN_INFO "%s():%d: " format, __func__, __LINE__, ##args); \
			} while(0)
#else
	#define DEBUG(format,args...) do { } while(0)
#endif


#define REG_SYSCTL    0x0000
#define REG_INTR    0x0006
#define REG_CLKCNF    0x000C
#define REG_CLKDIV1    0x000E
#define REG_CLKDIV2    0x0010

#define REG_GIOD    0x0040
#define REG_GIOA    0x0042

#define REG_AGM      0x010A
#define REG_FLFT    0x0110
#define REG_FRGT    0x0112
#define REG_FTOP    0x0114
#define REG_FBTM    0x0116
#define REG_FSTRX    0x0118
#define REG_FSTRY    0x011A
#define REG_VRAM    0x0202
#define REG_SSDCTL    0x0330
#define REG_SSD0    0x0332
#define REG_PSTCTL1    0x0400
#define REG_PSTCTL2    0x0402
#define REG_PTGCTL    0x042A
#define REG_PTHP    0x042C
#define REG_PTHB    0x042E
#define REG_PTHW    0x0430
#define REG_PTHF    0x0432
#define REG_PTVP    0x0434
#define REG_PTVB    0x0436
#define REG_PTVW    0x0438
#define REG_PTVF    0x043A
#define REG_VBLKS    0x0458
#define REG_VBLKE    0x045A
#define REG_SUBCTL    0x0700
#define REG_SUBTCMD    0x0702
#define REG_SUBTCMDD  0x0704
#define REG_REVBYTE    0x0A02
#define REG_REVCNT    0x0A04
#define REG_REVATTR    0x0A06
#define REG_REVFMT    0x0A08

#define CCI_SUB_UNKNOWN 0xffffffff
#define CCI_SUB_HYNIX 1
#define CCI_SUB_ROHM  2

//#define CLK_CTL_REG_BASE          0xA8600000
//#define HWIO_PMDH_NS_REG_ADDR    (CLK_CTL_REG_BASE  + 0x0000008c)

//===========  Power-ON sequence start

#define REG_EPSON_PWRCTL2 0x0011
#define VAL_EPSON_PWRCTL2 0x2003

#define REG_EPSON_PWRCTL5 0x0014
#define VAL_EPSON_PWRCTL5 0x000E

#define REG_EPSON_PWRCTL1 0x0010
#define VAL_EPSON_PWRCTL1 0x1E20

#define REG_EPSON_PWRCTL4_1 0x0013
#define VAL_EPSON_PWRCTL4_1 0x0040

//wait 20 ms or more

#define REG_EPSON_PWRCTL2 0x0011
#define VAL_EPSON_PWRCTL2 0x2003

//wait 100 ms or more

#define REG_EPSON_PWRCTL4_2 0x0013
#define VAL_EPSON_PWRCTL4_2 0x0060

//wait 80 ms or more

#define REG_EPSON_PWRCTL4_3 0x0013
#define VAL_EPSON_PWRCTL4_3 0x0070

#define REG_EPSON_DRVOUT 0x0001
#define VAL_EPSON_DRVOUT 0x0127

#define REG_EPSON_WAVCTL 0x0002
#define VAL_EPSON_WAVCTL 0x0700

#define REG_EPSON_ENTMOD 0x0003
#define VAL_EPSON_ENTMOD 0x1030

#define REG_EPSON_DISCTL1_1 0x0007
#define VAL_EPSON_DISCTL1_1 0x4800

#define REG_EPSON_DISCTL2 0x0008
#define VAL_EPSON_DISCTL2 0x8808

#define REG_EPSON_FRMCTL 0x000B
#define VAL_EPSON_FRMCTL 0x4200

#define REG_EPSON_GAMCTL1 0x0030
#define VAL_EPSON_GAMCTL1 0x0000

#define REG_EPSON_GAMCTL2 0x0031
#define VAL_EPSON_GAMCTL2 0x0304

#define REG_EPSON_GAMCTL3 0x0032
#define VAL_EPSON_GAMCTL3 0x0304

#define REG_EPSON_GAMCTL4 0x0033
#define VAL_EPSON_GAMCTL4 0x0000

#define REG_EPSON_GAMCTL5 0x0034
#define VAL_EPSON_GAMCTL5 0x0201

#define REG_EPSON_GAMCTL6 0x0035
#define VAL_EPSON_GAMCTL6 0x0304

#define REG_EPSON_GAMCTL7 0x0036
#define VAL_EPSON_GAMCTL7 0x0707

#define REG_EPSON_GAMCTL8 0x0037
#define VAL_EPSON_GAMCTL8 0x0000

#define REG_EPSON_GAMCTL9 0x0038
#define VAL_EPSON_GAMCTL9 0x0000

#define REG_EPSON_GAMCTL10 0x0039
#define VAL_EPSON_GAMCTL10 0x0000

#define REG_EPSON_SCRPOS1 0x0042
#define VAL_EPSON_SCRPOS1 0x013F

#define REG_EPSON_SCRPOS2 0x0043
#define VAL_EPSON_SCRPOS2 0x0000

#define REG_EPSON_GASCAN 0x0040
#define VAL_EPSON_GASCAN 0x0000

#define REG_EPSON_HADD 0x0046
#define VAL_EPSON_HADD 0xEF00

#define REG_EPSON_VADD1 0x0047
#define VAL_EPSON_VADD1 0x013F

#define REG_EPSON_VADD2 0x0048
#define VAL_EPSON_VADD2 0x0000

#define REG_EPSON_ADDSET1 0x0020
#define VAL_EPSON_ADDSET1 0x0000

#define REG_EPSON_ADDSET2 0x0021
#define VAL_EPSON_ADDSET2 0x0000

#define REG_EPSON_DISCTL1_2 0x0007
#define VAL_EPSON_DISCTL1_2 0x4812

//wait 80 ms or more

#define REG_EPSON_DISCTL1_3 0x0007
#define VAL_EPSON_DISCTL1_3 0x4813

//===========  Power-ON sequence end

//===========  Power-OFF sequence start

#define REG_EPSON_DISCTL1_DOWN1 0x0007
#define VAL_EPSON_DISCTL1_DOWN1 0x4812

// wait 80 ms or more

#define REG_EPSON_DISCTL1_DOWN2 0x0007
#define VAL_EPSON_DISCTL1_DOWN2 0x4800

#define REG_EPSON_PWRCTL1_DOWN1 0x0010
#define VAL_EPSON_PWRCTL1_DOWN1 0x0620

#define REG_EPSON_PWRCTL4_DOWN1 0x0013
#define VAL_EPSON_PWRCTL4_DOWN1 0x0060

// wait 120 ms or more

#define REG_EPSON_PWRCTL4_DOWN2 0x0013
#define VAL_EPSON_PWRCTL4_DOWN2 0x0040

// wait 100 ms or more

#define REG_EPSON_PWRCTL4_DOWN3 0x0013
#define VAL_EPSON_PWRCTL4_DOWN3 0x0000

// wait 100 ms or more

#define REG_EPSON_PWRCTL1_DOWN2 0x0010
#define VAL_EPSON_PWRCTL1_DOWN2 0x0001

int mddi_panel = 0;


static void mddi_cci_kb60_power_on_sequence(void)
{
	int rc;
	int err = 0;
	struct vreg *vreg_gp5 = NULL;

	vreg_gp5 = vreg_get(0, "gp5");

	//ahung@cci
	gpio_direction_output(LCD_DRV_EN,0);

	if(!DVT_1_3) {
		printk("%s HW VERSION : before DVT_1_3\n",__func__);
		rc = vreg_set_level(vreg_gp5, 2800);
		if (rc) {
			printk(KERN_ERR "%s: vreg set gp5 level failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
		rc = vreg_enable(vreg_gp5);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable gp2 failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
	}

	err = 0;
	gpio_direction_output(LCD_RST_N,0);
	msleep(50);
	gpio_direction_output(LCD_RST_N,1);
	msleep(100);

{
	rc = mddi_queue_register_write(0x2E80  ,0x01, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x2E80, &val, FALSE, 0);
//	if(val != 0x01)
//	printk("2E80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x0680  ,0x21, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x0680, &val, FALSE, 0);
//	if(val != 0x21)
//	printk("0x0680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD380  ,0x04, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD380, &val, FALSE, 0);
//	if(val != 0x04)
//	printk("0xD380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD480  ,0x5A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD480, &val, FALSE, 0);
//	if(val != 0x5A)
//	printk("0xD480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD580  ,0x07, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD580, &val, FALSE, 0);
//	if(val != 0x07)
//	printk("0xD580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD680  ,0x52, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD680, &val, FALSE, 0);
//	if(val != 0x52)
//	printk("0xD680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD080  ,0x17, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD080, &val, FALSE, 0);
//	if(val != 0x17)
//	printk("0xD080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD180  ,0x0D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD180, &val, FALSE, 0);
//	if(val != 0x0D)
//	printk("0xD180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD280  ,0x04, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD280, &val, FALSE, 0);
//	if(val != 0x04)
//	printk("0xD280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xDC80  ,0x04, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xDC80, &val, FALSE, 0);
//	if(val != 0x04)
//	printk("0xDC80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xD780  ,0x01, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xD780, &val, FALSE, 0);
//	if(val != 0x01)
//	printk("0xD780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x2280  ,0x09, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x2280, &val, FALSE, 0);
//	if(val != 0x09)
//	printk("0x2280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x2480  ,0x4C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x2480, &val, FALSE, 0);
//	if(val != 0x4C)
//	printk("0x2480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x2580  ,0x30, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x2580, &val, FALSE, 0);
//	if(val != 0x30)
//	printk("0x2580 = 0x%08x\n",val);


	rc = mddi_queue_register_write(0x2780  ,0x7A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x2780, &val, FALSE, 0);
//	if(val != 0x7A)
//	printk("0x2780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x0180  ,0x02, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x0180, &val, FALSE, 0);
//	if(val != 0x02)
//	printk("0x0180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4080  ,0x07, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4080, &val, FALSE, 0);
//	if(val != 0x07)
//	printk("0x4080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4180  ,0x0A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4180, &val, FALSE, 0);
//	if(val != 0x0A)
//	printk("0x4180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4280  ,0x13, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4280, &val, FALSE, 0);
//	if(val != 0x13)
//	printk("0x4280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4380  ,0x1D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4380, &val, FALSE, 0);
//	if(val != 0x1D)
//	printk("0x4380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4480  ,0x15, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4480, &val, FALSE, 0);
//	if(val != 0x15)
//	printk("0x4480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4580  ,0x27, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4580, &val, FALSE, 0);
//	if(val != 0x27)
//	printk("0x4580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4680  ,0x5B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4680, &val, FALSE, 0);
//	if(val != 0x5B)
//	printk("0x4680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4780  ,0x12, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4780, &val, FALSE, 0);
//	if(val != 0x12)
//	printk("0x4780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4880  ,0x22, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4880, &val, FALSE, 0);
//	if(val != 0x22)
//	printk("0x4880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4980  ,0x27, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4980, &val, FALSE, 0);
//	if(val != 0x27)
//	printk("0x4980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4A80  ,0x64, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4A80, &val, FALSE, 0);
//	if(val != 0x64)
//	printk("0x4A80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4B80  ,0x21, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4B80, &val, FALSE, 0);
//	if(val != 0x21)
//	printk("0x4B80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4C80  ,0x4E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4C80, &val, FALSE, 0);
//	if(val != 0x4E)
//	printk("0x4C80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4D80  ,0x5E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4D80, &val, FALSE, 0);
//	if(val != 0x5E)
//	printk("0x4D80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4E80  ,0x42, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4E80, &val, FALSE, 0);
//	if(val != 0x42)
//	printk("0x4E80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x4F80  ,0x7A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x4F80, &val, FALSE, 0);
//	if(val != 0x7A)
//	printk("0x4F80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5080  ,0x30, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5080, &val, FALSE, 0);
//	if(val != 0x30)
//	printk("0x5080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5180  ,0x43, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5180, &val, FALSE, 0);
//	if(val != 0x43)
//	printk("0x5180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5880  ,0x0B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5880, &val, FALSE, 0);
//	if(val != 0x0B)
//	printk("0x5880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5980  ,0x1E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5980, &val, FALSE, 0);
//	if(val != 0x1E)
//	printk("0x5980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5A80  ,0x5A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5A80, &val, FALSE, 0);
//	if(val != 0x5A)
//	printk("0x5A80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5B80  ,0x8E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5B80, &val, FALSE, 0);
//	if(val != 0x8E)
//	printk("0x5B80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5C80  ,0x22, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5C80, &val, FALSE, 0);
//	if(val != 0x22)
//	printk("0x5C80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5D80  ,0x32, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5D80, &val, FALSE, 0);
//	if(val != 0x32)
//	printk("0x5D80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5E80  ,0x5D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5E80, &val, FALSE, 0);
//	if(val != 0x5D)
//	printk("0x5E80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5F80  ,0x63, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x5F80, &val, FALSE, 0);
//	if(val != 0x63)
//	printk("0x5F80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6080  ,0x19, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6080, &val, FALSE, 0);
//	if(val != 0x19)
//	printk("0x6080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6180  ,0x20, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6180, &val, FALSE, 0);
//	if(val != 0x20)
//	printk("0x6180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6280  ,0xB7, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6280, &val, FALSE, 0);
//	if(val != 0xB7)
//	printk("0x6280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6380  ,0x25, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6380, &val, FALSE, 0);
//	if(val != 0x25)
//	printk("0x6380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6480  ,0x57, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6480, &val, FALSE, 0);
//	if(val != 0x57)
//	printk("0x6480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6580  ,0x6B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6580, &val, FALSE, 0);
//	if(val != 0x6B)
//	printk("0x6580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6680  ,0xA9, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6680, &val, FALSE, 0);
//	if(val != 0xA9)
//	printk("0x6680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6780  ,0xB3, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6780, &val, FALSE, 0);
//	if(val != 0xB3)
//	printk("0x6780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6880  ,0x3D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6880, &val, FALSE, 0);
//	if(val != 0x3D)
//	printk("0x6880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6980  ,0x40, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x6980, &val, FALSE, 0);
//	if(val != 0x40)
//	printk("0x6980 = 0x%08x\n",val);


	rc = mddi_queue_register_write(0x7080  ,0x07, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7080, &val, FALSE, 0);
//	if(val != 0x07)
//	printk("0x7080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7180  ,0x0B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7180, &val, FALSE, 0);
//	if(val != 0x0B)
//	printk("0x7180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7280  ,0x16, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7280, &val, FALSE, 0);
//	if(val != 0x16)
//	printk("0x7280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7380  ,0x22, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7380, &val, FALSE, 0);
//	if(val != 0x22)
//	printk("0x7380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7480  ,0x17, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7480, &val, FALSE, 0);
//	if(val != 0x17)
//	printk("0x7480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7580  ,0x2A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7580, &val, FALSE, 0);
//	if(val != 0x2A)
//	printk("0x7580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7680  ,0x5D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7680, &val, FALSE, 0);
//	if(val != 0x5D)
//	printk("0x7680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7780  ,0x20, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7780, &val, FALSE, 0);
//	if(val != 0x20)
//	printk("0x7780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7880  ,0x21, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7880, &val, FALSE, 0);
//	if(val != 0x21)
//	printk("0x7880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7980  ,0x28, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7980, &val, FALSE, 0);
//	if(val != 0x28)
//	printk("0x7980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7A80  ,0x6F, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7A80, &val, FALSE, 0);
//	if(val != 0x6F)
//	printk("0x7A80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7B80  ,0x1D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7B80, &val, FALSE, 0);
//	if(val != 0x1D)
//	printk("0x7B80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7C80  ,0x46, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7C80, &val, FALSE, 0);
//	if(val != 0x46)
//	printk("0x7C80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7D80  ,0x59, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7D80, &val, FALSE, 0);
//	if(val != 0x59)
//	printk("0x7D80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7E80  ,0x54, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7E80, &val, FALSE, 0);
//	if(val != 0x54)
//	printk("0x7E80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x7F80  ,0x70, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x7F80, &val, FALSE, 0);
//	if(val != 0x70)
//	printk("0x7F80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8080  ,0x2E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8080, &val, FALSE, 0);
//	if(val != 0x2E)
//	printk("0x8080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8180  ,0x43, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8180, &val, FALSE, 0);
//	if(val != 0x43)
//	printk("0x8180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8880  ,0x0B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8880, &val, FALSE, 0);
//	if(val != 0x0B)
//	printk("0x8880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8980  ,0x21, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8980, &val, FALSE, 0);
//	if(val != 0x21)
//	printk("0x8980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8A80  ,0x62, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8A80, &val, FALSE, 0);
//	if(val != 0x62)
//	printk("0x8A80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8B80  ,0x7C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8B80, &val, FALSE, 0);
//	if(val != 0x7C)
//	printk("0x8B80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8C80  ,0x26, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8C80, &val, FALSE, 0);
//	if(val != 0x26)
//	printk("0x8C80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8D80  ,0x3B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8D80, &val, FALSE, 0);
//	if(val != 0x3B)
//	printk("0x8D80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8E80  ,0x62, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8E80, &val, FALSE, 0);
//	if(val != 0x62)
//	printk("0x8E80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x8F80  ,0x58, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x8F80, &val, FALSE, 0);
//	if(val != 0x58)
//	printk("0x8F80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9080  ,0x18, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9080, &val, FALSE, 0);
//	if(val != 0x18)
//	printk("0x9080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9180  ,0x20, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9180, &val, FALSE, 0);
//	if(val != 0x20)
//	printk("0x9180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9280  ,0xAA, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9280, &val, FALSE, 0);
//	if(val != 0xAA)
//	printk("0x9280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9380  ,0x23, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9380, &val, FALSE, 0);
//	if(val != 0x23)
//	printk("0x9380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9480  ,0x54, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9480, &val, FALSE, 0);
//	if(val != 0x54)
//	printk("0x9480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9580  ,0x69, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9580, &val, FALSE, 0);
//	if(val != 0x69)
//	printk("0x9580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9680  ,0xA4, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9680, &val, FALSE, 0);
//	if(val != 0xA4)
//	printk("0x9680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9780  ,0xB0, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9780, &val, FALSE, 0);
//	if(val != 0xB0)
//	printk("0x9780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9880  ,0x3C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9880, &val, FALSE, 0);
//	if(val != 0x3C)
//	printk("0x9880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x9980  ,0x40, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x9980, &val, FALSE, 0);
//	if(val != 0x40)
//	printk("0x9980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA080  ,0x07, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA080, &val, FALSE, 0);
//	if(val != 0x07)
//	printk("0xA080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA180  ,0x0C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA180, &val, FALSE, 0);
//	if(val != 0x0C)
//	printk("0xA180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA280  ,0x1B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA280, &val, FALSE, 0);
//	if(val != 0x1B)
//	printk("0xA280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA380  ,0x2C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA380, &val, FALSE, 0);
//	if(val != 0x2C)
//	printk("0xA380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA480  ,0x1A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA480, &val, FALSE, 0);
//	if(val != 0x1A)
//	printk("0xA480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA580  ,0x2E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA580, &val, FALSE, 0);
//	if(val != 0x2E)
//	printk("0xA580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA680  ,0x60, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA680, &val, FALSE, 0);
//	if(val != 0x60)
//	printk("0xA680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA780  ,0x31, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA780, &val, FALSE, 0);
//	if(val != 0x31)
//	printk("0xA780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA880  ,0x21, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA880, &val, FALSE, 0);
//	if(val != 0x21)
//	printk("0xA880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xA980  ,0x27, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xA980, &val, FALSE, 0);
//	if(val != 0x27)
//	printk("0xA980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAA80  ,0x7D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAA80, &val, FALSE, 0);
//	if(val != 0x7D)
//	printk("0xAA80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAB80  ,0x10, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAB80, &val, FALSE, 0);
//	if(val != 0x10)
//	printk("0xAB80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAC80  ,0x29, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAC80, &val, FALSE, 0);
//	if(val != 0x29)
//	printk("0xAC80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAD80  ,0x38, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAD80, &val, FALSE, 0);
//	if(val != 0x38)
//	printk("0xAD80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAE80  ,0xAD, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAE80, &val, FALSE, 0);
//	if(val != 0xAD)
//	printk("0xAE80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xAF80  ,0xC4, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xAF80, &val, FALSE, 0);
//	if(val != 0xC4)
//	printk("0xAF80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xB080  ,0x44, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xB080, &val, FALSE, 0);
//	if(val != 0x44)
//	printk("0xB080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xB180  ,0x43, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xB180, &val, FALSE, 0);
//	if(val != 0x43)
//	printk("0xB180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xB880  ,0x0A, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xB880, &val, FALSE, 0);
//	if(val != 0x0A)
//	printk("0xB880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xB980  ,0x0B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xB980, &val, FALSE, 0);
//	if(val != 0x0B)
//	printk("0xB980 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBA80  ,0x0D, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBA80, &val, FALSE, 0);
//	if(val != 0x0D)
//	printk("0xBA80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBB80  ,0x42, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBB80, &val, FALSE, 0);
//	if(val != 0x42)
//	printk("0xBB80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBC80  ,0x3F, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBC80, &val, FALSE, 0);
//	if(val != 0x3F)
//	printk("0xBC80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBD80  ,0x52, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBD80, &val, FALSE, 0);
//	if(val != 0x52)
//	printk("0xBD80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBE80  ,0x6B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBE80, &val, FALSE, 0);
//	if(val != 0x6B)
//	printk("0xBE80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xBF80  ,0x4E, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xBF80, &val, FALSE, 0);
//	if(val != 0x4E)
//	printk("0xBF80 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC080  ,0x19, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC080, &val, FALSE, 0);
//	if(val != 0x19)
//	printk("0xC080 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC180  ,0x1F, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC180, &val, FALSE, 0);
//	if(val != 0x1F)
//	printk("0xC180 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC280  ,0x9C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC280, &val, FALSE, 0);
//	if(val != 0x9C)
//	printk("0xC280 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC380  ,0x20, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC380, &val, FALSE, 0);
//	if(val != 0x20)
//	printk("0xC380 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC480  ,0x51, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC480, &val, FALSE, 0);
//	if(val != 0x51)
//	printk("0xC480 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC580  ,0x67, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC580, &val, FALSE, 0);
//	if(val != 0x67)
//	printk("0xC580 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC680  ,0x98, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC680, &val, FALSE, 0);
//	if(val != 0x98)
//	printk("0xC680 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC780  ,0xAC, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC780, &val, FALSE, 0);
//	if(val != 0xAC)
//	printk("0xC780 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC880  ,0x3B, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC880, &val, FALSE, 0);
//	if(val != 0x3B)
//	printk("0xC880 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0xC980  ,0x40, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0xC980, &val, FALSE, 0);
//	if(val != 0x40)
//	printk("0xC980 = 0x%08x\n",val);



	rc = mddi_queue_register_write(0x3500  ,0x00, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x3500, &val, FALSE, 0);
//	if(val != 0x00)
//	printk("0x3500 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x5300  ,0x2C, FALSE, 0);
	if( rc < 0 )
	    err = 1;
////	mddi_queue_register_read(0x5300, &val, FALSE, 0);
////	if(val != 0x2C)
////	printk("0x5300 = 0x%08x\n",val);

//	rc = mddi_queue_register_write(0x5500  ,0x02, FALSE, 0);
//	if( rc < 0 )
//	    err = 1;
//	mddi_queue_register_write(0x5301  ,0x00, FALSE, 0);	//CLED_VOL = 0,PWM_ENH_OE=0
	rc = mddi_queue_register_write(0x5301  ,0x00, FALSE, 0);//sync to kb60 avoid lcm flick
	if( rc < 0 )
		err = 1;
	
	rc = mddi_queue_register_write(0x6A17  ,0x01, FALSE, 0);
	if( rc < 0 )
		err = 1;
//	mddi_queue_register_read(0x6A17, &val, FALSE, 0);
//	if(val != 0x01)
//	printk("0x6A17 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x6A18  ,backlight_val, FALSE, 0);
	if( rc < 0 )
		err = 1;
////	mddi_queue_register_read(0x6A18, &val, FALSE, 0);
////	if(val != 0x4C)
////	printk("0x6A18 = 0x%08x\n",val);

//	mddi_queue_register_write(0x5307  ,0x01, FALSE, 0);
	rc = mddi_queue_register_write(0x5308  ,0x01, FALSE, 0);
	if( rc < 0 )
	    err = 1;
////	mddi_queue_register_read(0x5308, &val, FALSE, 0);
////	if(val != 0x01)
////	printk("0x5308 = 0x%08x\n",val);

	rc = mddi_queue_register_write(0x3600  ,0x03, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x3600, &val, FALSE, 0);
//	if(val != 0x03)
//	printk("0x3600 = 0x%08x\n",val);


	rc = mddi_queue_register_write(0x1100  ,0x00, FALSE, 0);
	if( rc < 0 )
	    err = 1;
//	mddi_queue_register_read(0x1100, &val, FALSE, 0);
//	if(val != 0x00)
//	printk("0x1100 = 0x%08x\n",val);

	if( initial )
		mdelay(250);
	else
		initial = 1;

//	rc = mddi_queue_register_write(0x2900  ,0x00, FALSE, 0);
//	if( rc < 0 )
//	    err = 1;
//	mddi_queue_register_read(0x2900, &val, FALSE, 0);
//	if(val != 0x00)
//	printk("0x2900 = 0x%08x\n",val);
}
	return;
}

void cci_not_allow_powerdown(void){
	DEBUG("##### %s #####\n",__func__);
	mddi_cci_kb60_not_allow_off = TRUE;
}
EXPORT_SYMBOL(cci_not_allow_powerdown);
void cci_allow_powerdown(void){
	DEBUG("##### %s #####\n",__func__);
	mddi_cci_kb60_not_allow_off = FALSE;
}
EXPORT_SYMBOL(cci_allow_powerdown);

void turnon_bkl(void) {
  if( mddi_cci_kb60_backlight_is_off ){
    msleep(50);
	mddi_queue_register_write(0x2900  ,0x00, FALSE, 0);
    gpio_direction_output(LCD_DRV_EN,1);
    mddi_cci_kb60_backlight_is_off = FALSE;
  }
}
EXPORT_SYMBOL(turnon_bkl);

static void mddi_cci_kb60_power_off_sequence(void)
{
	int rc;
	struct vreg *vreg_gp5 = NULL;

	rc = 0;
	vreg_gp5 = vreg_get(0, "gp5");

	gpio_direction_output(LCD_DRV_EN,0);

	if(!DVT_1_3) {
		printk("%s HW VERSION : before DVT_1_3\n",__func__);
		mddi_queue_register_write(0x2800  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x1000  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x4F00  ,0x01, TRUE, 0);
		gpio_direction_output(LCD_RST_N,0);
		msleep(30);
		rc = vreg_disable(vreg_gp5);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable gp2 failed (%d)\n",
			       __func__, rc);
			//return -EIO;
		}
	}
	else {
		printk("%s HW VERSION : after DVT_1_3\n",__func__);
		mddi_queue_register_write(0x2800  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x2800  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x2800  ,0x00, TRUE, 0);
		mdelay(100);
		mddi_queue_register_write(0x1000  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x1000  ,0x00, TRUE, 0);
		mddi_queue_register_write(0x1000  ,0x00, TRUE, 0);
		mdelay(100);
		mddi_queue_register_write(0x4F00  ,0x01, TRUE, 0);
	}
}
//===========  Power-OFF sequence end

static void mddi_cci_kb60_lcd_powerdown(void)
{
  	printk("call mddi_cci_kb60_power_off_sequence\n");
	mddi_cci_kb60_power_off_sequence();
}
static void mddi_cci_kb60_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
#if 1
	//uint32 regdata=0x0;
	int32 level=0x0;
//	uint32 val;
        //uint32 id=0x0;
	DEBUG("######## Config LCM_BL_EN GPIO 91 level = %d ############## \n", mfd->bl_level);

	level = mfd->bl_level;

	//james@cci added for backlight control
	if(level == 7 /*&& mddi_cci_kb60_backlight_is_off*/) {
		//gpio_direction_output(LCD_DRV_EN,1);
		DEBUG("do backlighton\n");
		//mddi_cci_kb60_backlight_is_off = FALSE;
	}
	//james@cci added for backlight control
	if(level == 0) {//change to 1
		if(mddi_cci_kb60_not_allow_off)
			return;
		gpio_direction_output(LCD_DRV_EN,0);

		//disable_mpp9(1);//turn off key light

		DEBUG("do backlightoff\n");
		mddi_cci_kb60_backlight_is_off = TRUE;
	}
#endif
}

static void mddi_cci_kb60_prim_lcd_init(void)
{
  	DEBUG("call mddi_cci_kb60_power_on_sequence...\n");
  	mddi_cci_kb60_power_on_sequence();
	DEBUG("mddi_host_write_pix_attr_reg(MDDI_DEFAULT_PRIM_PIX_ATTR) Set the MDP pixel data attributes for Primary Display\n");
	/* Set the MDP pixel data attributes for Primary Display */
	mddi_host_write_pix_attr_reg(MDDI_DEFAULT_PRIM_PIX_ATTR);
	return;
}

#if 0
void mddi_cci_kb60_lcd_vsync_detected(boolean detected)
{
	// static timetick_type start_time = 0;
	static struct timeval start_time;
	static boolean first_time = TRUE;
	// uint32 mdp_cnt_val = 0;
	// timetick_type elapsed_us;
	struct timeval now;
	uint32 elapsed_us;
	uint32 num_vsyncs;

	DEBUG("##### %s #####\n",__func__);
	if ((detected) || (mddi_cci_kb60_vsync_attempts > 5)) {
		if ((detected) && (mddi_cci_kb60_monitor_refresh_value)) {
			// if (start_time != 0)
			if (!first_time) {
				// elapsed_us = timetick_get_elapsed(start_time, T_USEC);
				jiffies_to_timeval(jiffies, &now);
				elapsed_us =
				    (now.tv_sec - start_time.tv_sec) * 1000000 +
				    now.tv_usec - start_time.tv_usec;
				/* LCD is configured for a refresh every * usecs, so to
				 * determine the number of vsyncs that have occurred
				 * since the last measurement add half that to the
				 * time difference and divide by the refresh rate. */
				num_vsyncs = (elapsed_us +
					      (mddi_cci_kb60_usecs_per_refresh >>
					       1)) /
				    mddi_cci_kb60_usecs_per_refresh;
				/* LCD is configured for * hsyncs (rows) per refresh cycle.
				 * Calculate new rows_per_second value based upon these
				 * new measurements. MDP can update with this new value. */
				mddi_cci_kb60_rows_per_second =
				    (mddi_cci_kb60_rows_per_refresh * 1000 *
				     num_vsyncs) / (elapsed_us / 1000);
			}
			// start_time = timetick_get();
			first_time = FALSE;
			jiffies_to_timeval(jiffies, &start_time);
			if (mddi_cci_kb60_report_refresh_measurements) {
				// mdp_cnt_val = MDP_LINE_COUNT;
			}
		}
		/* if detected = TRUE, client initiated wakeup was detected */
		if (mddi_cci_kb60_vsync_handler != NULL) {
			(*mddi_cci_kb60_vsync_handler)
			    (mddi_cci_kb60_vsync_handler_arg);
			mddi_cci_kb60_vsync_handler = NULL;
		}
		mddi_vsync_detect_enabled = FALSE;
		mddi_cci_kb60_vsync_attempts = 0;
		/* need to clear this vsync wakeup */
		if (!mddi_queue_register_write_int(REG_INTR, 0x0000)) {
			MDDI_MSG_ERR("Vsync interrupt clear failed!\n");
		}
		if (!detected) {
			/* give up after 5 failed attempts but show error */
			//james@cci mark
			//MDDI_MSG_NOTICE("Vsync detection failed!\n");
		} else if ((mddi_cci_kb60_monitor_refresh_value) &&
			   (mddi_cci_kb60_report_refresh_measurements)) {
//      MDDI_MSG_NOTICE("  MDP Line Counter=%d!\n",mdp_cnt_val);
			MDDI_MSG_NOTICE("  Lines Per Second=%d!\n",
					mddi_cci_kb60_rows_per_second);
		}
	} else {
		/* if detected = FALSE, we woke up from hibernation, but did not
		 * detect client initiated wakeup.
		 */
		mddi_cci_kb60_vsync_attempts++;
	}
}


void mddi_cci_kb60_vsync_set_handler(msm_fb_vsync_handler_type handler,	/* ISR to be executed */
				  void *arg)
{
	boolean error = FALSE;
	unsigned long flags;

	DEBUG("##### %s #####\n",__func__);
	/* Disable interrupts */
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	// INTLOCK();

	if (mddi_cci_kb60_vsync_handler != NULL) {
		error = TRUE;
	}

	/* Register the handler for this particular GROUP interrupt source */
	mddi_cci_kb60_vsync_handler = handler;
	mddi_cci_kb60_vsync_handler_arg = arg;

	/* Restore interrupts */
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
	// INTFREE();

	if (error) {
		MDDI_MSG_ERR("MDDI: Previous Vsync handler never called\n");
	}

	/* Enable the vsync wakeup */
	//james@cci mark
	mddi_queue_register_write(REG_INTR, 0x8100, FALSE, 0);

	mddi_cci_kb60_vsync_attempts = 1;
	mddi_vsync_detect_enabled = TRUE;
}				/* mddi_cci_kb60_vsync_set_handler */
#endif

static int mddi_cci_kb60_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

    printk("########## %s ##########\n",__FUNCTION__);
#if 0 //james@cci
	if(!mddi_cci_kb60_lcd_is_off){

        printk("%s lcd is off , return 0\n",__FUNCTION__);
		return 0;
    }
#endif
	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

    printk("panel.id = %d\n",mfd->panel.id);
	if (mfd->panel.id == CCI_KB60_WVGA_PRIM) {
        printk("%s\n",__FUNCTION__);
		mddi_cci_kb60_prim_lcd_init();
	} else {
		printk(KERN_ALERT "%s:%d --> Unknown LCD Panel id = %d, return -EINVAL\n", __func__, __LINE__, mfd->panel.id);
		return -EINVAL;
	}
	//james@cci , //mddi_cci_kb60_lcd_is_off = FALSE;
	return 0;
}

static int mddi_cci_kb60_lcd_off(struct platform_device *pdev)
{
	if(!mddi_cci_kb60_not_allow_off)
	{
		mddi_cci_kb60_lcd_powerdown();
		//mddi_cci_kb60_lcd_is_off = TRUE; //james@cci
	}
	return 0;
}


//static ssize_t panel_backlight_set(struct device *dev,
//				     struct device_attribute *attr,
//				     const char *buf, size_t count)
//{
//	uint32 set_backlight = 0;
//	uint32 val = 0;
//
//	set_backlight = simple_strtoul(buf, NULL, 10);
//
//	if (set_backlight > 255)
//		return -EINVAL;
//
//	mddi_queue_register_write(0x6A18  ,set_backlight, TRUE, 0);
//	mddi_queue_register_read(0x6A00, &val, TRUE, 0);
//	printk("0x6A00 = %x\n",val);
//	return 0;
//}
//
//static struct device_attribute cci_set_backlight_attr[] = {
//	__ATTR(set_backlight, 0644, NULL, panel_backlight_set),
//};
//
//static int backlight_register_sysfs(struct device * dev)
//{
//	int i, err;
//
//	for (i = 0; i < ARRAY_SIZE(cci_set_backlight_attr); i++) {
//		err = device_create_file(dev, &cci_set_backlight_attr[i]);
//		if (err)
//			goto err_out;
//	}
//
//	return 0;
//
//err_out:
//	while(i--)
//		device_remove_file(dev, &cci_set_backlight_attr[i]);
//	return err;
//}



static int __init mddi_cci_kb60_probe(struct platform_device *pdev)
{
	DEBUG("##### %s #####\n",__func__);

	if (pdev->id == 0) {
		DEBUG("##### %s id == 0 #####\n",__func__);
		mddi_cci_kb60_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);
	DEBUG("##### %s msm_fb_add_device #####\n",__func__);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mddi_cci_kb60_probe,
	.driver = {
		.name   = "mddi_cci_kb60_wvga",
	},
};


static struct msm_fb_panel_data mddi_cci_kb60_panel_data = {
	.on = mddi_cci_kb60_lcd_on,
	.off = mddi_cci_kb60_lcd_off,
	.set_backlight = mddi_cci_kb60_lcd_set_backlight,
	.set_vsync_notifier = NULL/*mddi_cci_kb60_vsync_set_handler*/,
};

static struct platform_device this_device = {
	.name   = "mddi_cci_kb60_wvga",
	.id	= CCI_KB60_WVGA_PRIM,
	.dev	= {
		.platform_data = &mddi_cci_kb60_panel_data,
	}
};

#if CHECK_HWID
static int hw_version_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	char *out = page;
	int len;

	out += sprintf(out, "%d\n", HWVERSION);

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

static int band_id_read_proc(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	char *out = page;
	int len;

	out += sprintf(out, "%02x\n", BANDID);

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
#endif

static int __init mddi_cci_kb60_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;
#if CHECK_HWID
	struct proc_dir_entry *d_entry;
	struct proc_dir_entry *band_entry;
	int HWID14 = 0;
	int HWID15 = 0;
	int HWID16 = 0;
//	int HWID17 = 0;
	int HWID21 = 1;
	int HWID22 = 1;
	struct mpp *mpp_14 = NULL;
	struct mpp *mpp_15 = NULL;
	struct mpp *mpp_16 = NULL;
//	struct mpp *mpp_17 = NULL;
	struct mpp *mpp_21 = NULL;
	struct mpp *mpp_22 = NULL;
	struct msm_rpc_endpoint *cci_ep;
	int rc;
	struct rpc_req_id {
		struct rpc_request_hdr hdr;
		int mpp;
	} req_hw;

	struct rpc_reply_id {
		struct rpc_reply_hdr hdr;
		u32 id;
	} rep_hw;

	mpp_14 = mpp_get(NULL, "mpp14");
	rpc_mpp_config_digital_input(mpp_14, PM_MPP__DLOGIC__LVL_VDD,
								PM_MPP__DLOGIC_IN__DBUS_NONE);
	mpp_15 = mpp_get(NULL, "mpp15");
	rpc_mpp_config_digital_input(mpp_15, PM_MPP__DLOGIC__LVL_VDD,
								PM_MPP__DLOGIC_IN__DBUS_NONE);
	mpp_16 = mpp_get(NULL, "mpp16");
	rpc_mpp_config_digital_input(mpp_16, PM_MPP__DLOGIC__LVL_VDD,
								PM_MPP__DLOGIC_IN__DBUS_NONE);
//	mpp_17 = mpp_get(NULL, "mpp17");
//	rpc_mpp_config_digital_input(mpp_17, PM_MPP__DLOGIC__LVL_VDD,
//								PM_MPP__DLOGIC_IN__DBUS_NONE);

	mpp_21 = mpp_get(NULL, "mpp21");
	rpc_mpp_config_digital_input(mpp_21, PM_MPP__DLOGIC__LVL_MSMP,
								PM_MPP__DLOGIC_IN__DBUS_NONE);
	mpp_22 = mpp_get(NULL, "mpp22");
	rpc_mpp_config_digital_input(mpp_22, PM_MPP__DLOGIC__LVL_MSMP,
								PM_MPP__DLOGIC_IN__DBUS_NONE);

	cci_ep = msm_rpc_connect(CCIPROG1, CCIVERS1, 0);
	if (cci_ep == NULL) {
		printk( "%s: cci_ep = NULL\n",__func__);
	} else if (IS_ERR(cci_ep)) {
		rc = PTR_ERR(cci_ep);
		printk(KERN_ERR
		       "%s: rpc connect failed for CCI_RPC_PROG. rc = %d\n",
		       __func__, rc);
		cci_ep = NULL;
	}
	else {
		req_hw.mpp = cpu_to_be32(MPP_14);
		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
		if (rc < 0) {
			printk("%s: HWID14 error\n", __func__);
		}
		else {
			HWID14 = be32_to_cpu(rep_hw.id);
			printk("%s: HWID14 = %d\n", __func__,HWID14);
		}
//
		req_hw.mpp = cpu_to_be32(MPP_15);
		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
		if (rc < 0) {
			printk("%s: HWID15 error\n", __func__);
		}
		else {
			HWID15 = be32_to_cpu(rep_hw.id);
			printk("%s: HWID15 = %d\n", __func__,HWID15);
		}
//
		req_hw.mpp = cpu_to_be32(MPP_16);
		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
		if (rc < 0) {
			printk("%s: HWID16 error\n", __func__);
		}
		else {
			HWID16 = be32_to_cpu(rep_hw.id);
			printk("%s: HWID16 = %d\n", __func__,HWID16);
		}
//
//		req_hw.mpp = cpu_to_be32(MPP_17);
//		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
//					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
//		if (rc < 0) {
//			printk("%s: HWID17 error\n", __func__);
//		}
//		else {
//			HWID17 = be32_to_cpu(rep_hw.id);
//			if(HWID17 == 1)
//				HW_ID4 = 1;
//			printk("%s: HWID17 = %d\n", __func__,HWID17);
//		}
//
		req_hw.mpp = cpu_to_be32(MPP_21);
		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
		if (rc < 0) {
			printk("%s: HWID21 error\n", __func__);
		}
		else {
			HWID21 = be32_to_cpu(rep_hw.id);
			printk("%s: HWID21 = %d\n", __func__,HWID21);
		}
		req_hw.mpp = cpu_to_be32(MPP_22);
		rc = msm_rpc_call_reply(cci_ep,	CCI_READ_MPP_DIGITAL_INPUT,
					&req_hw, sizeof(req_hw),&rep_hw, sizeof(rep_hw),5*HZ);
		if (rc < 0) {
			printk("%s: HWID22 error\n", __func__);
		}
		else {
			HWID22 = be32_to_cpu(rep_hw.id);
			printk("%s: HWID22 = %d\n", __func__,HWID22);
		}
		BANDID = (HWID21<<1) | (HWID22);
//
		if( (HWID14==0) && (HWID15==1) && (HWID16==0) ) {
			DVT_1_3 = 1;
			HWVERSION = (HWID14<<2) | (HWID15<<1) | (HWID16);
			printk("%s HW VERSION : DVT_1_3\n",__func__);
		} else if( (HWID14==0) && (HWID15==1) && (HWID16==1) ) {
			DVT_1_3 = 1;
			DVT_2 = 1;
			HWVERSION = (HWID14<<2) | (HWID15<<1) | (HWID16);
			printk("%s HW VERSION : DVT_2\n",__func__);
		} else if( ((HWID14==1) && (HWID15==0) && (HWID16==0)) 
		//B [Bugzilla 150] Add new hardware id, Jiahan_Li
		                ||((HWID14==1) && (HWID15==0) && (HWID16==1)) ) {
		//E [Bugzilla 150] Add new hardware id, Jiahan_Li
			DVT_1_3 = 1;
			DVT_2 = 1;
			PVT = 1;
			HW_ID4 = 1;
			HWVERSION = (HWID14<<2) | (HWID15<<1) | (HWID16);
			printk("%s HW VERSION : KB62_DVT1\n",__func__);
		} else {
			HWVERSION = 0;
			printk("%s HW VERSION : before DVT_1_3\n",__func__);
		}

		d_entry = create_proc_entry("hw_version",
		                   S_IRUGO | S_IWUSR | S_IWGRP, NULL);
		if (d_entry) {
		    d_entry->read_proc = hw_version_read_proc;
		    d_entry->write_proc = NULL;
		    d_entry->data = NULL;
		}

		band_entry = create_proc_entry("band_id",
		                   S_IRUGO | S_IWUSR | S_IWGRP, NULL);
		if (band_entry) {
		    band_entry->read_proc = band_id_read_proc;
		    band_entry->write_proc = NULL;
		    band_entry->data = NULL;
		}
	}
#endif

//#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
//	u32 id;
//	id = mddi_get_client_id();
//
//	if (((id >> 16) != 0x0) || ((id & 0xffff) != 0x8835))
//		return 0;
//#endif
    printk("%s\n",__FUNCTION__);
	if (gpio_request(LCD_RST_N, "lcd_rst_n"))
		printk(KERN_ERR "failed to request gpio lcd_rst_n\n");
	gpio_configure(LCD_RST_N,GPIOF_DRIVE_OUTPUT);

	if (gpio_request(LCD_DRV_EN, "lcd_drv_en"))
		printk(KERN_ERR "failed to request gpio lcd_drv_en\n");
	gpio_configure(LCD_DRV_EN,GPIOF_DRIVE_OUTPUT);

	if (mddi_host_core_version > 8) {
		/* can use faster refresh with newer hw revisions */
		mddi_cci_kb60_debug_60hz_refresh = TRUE;

		/* Timing variables for tracking vsync */
		/* dot_clock = 6.00MHz
		 * horizontal count = 296
		 * vertical count = 338
		 * refresh rate = 6000000/(296+338) = 60Hz
		 */
		mddi_cci_kb60_rows_per_second = 20270;	/* 6000000/296 */
		mddi_cci_kb60_rows_per_refresh = 338;
		mddi_cci_kb60_usecs_per_refresh = 16674;	/* (296+338)/6000000 */
		DEBUG("##### %s mddi_host_core_version > 8 #####\n",__func__);
	} else {
		/* Timing variables for tracking vsync */
		/* dot_clock = 5.20MHz
		 * horizontal count = 376
		 * vertical count = 338
		 * refresh rate = 5200000/(376+338) = 41Hz
		 */
		mddi_cci_kb60_rows_per_second = 13830;	/* 5200000/376 */
		mddi_cci_kb60_rows_per_refresh = 338;
		mddi_cci_kb60_usecs_per_refresh = 24440;	/* (376+338)/5200000 */

	}
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &mddi_cci_kb60_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = MDDI_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 160000000;//153600000;//122880000;
	pinfo->clk_min = 160000000;//150000000;//120000000;
	pinfo->clk_max = 160000000;//160000000;//125000000;
	pinfo->lcd.vsync_enable = TRUE;//TRUE;
	pinfo->lcd.refx100 =
		(mddi_cci_kb60_rows_per_second * 100) /
		mddi_cci_kb60_rows_per_refresh;
	pinfo->lcd.v_back_porch = 14;//8;
	pinfo->lcd.v_front_porch = 2;//8;
	pinfo->lcd.v_pulse_width = 0;//0;
	pinfo->lcd.hw_vsync_mode = TRUE;//TRUE;
	pinfo->lcd.vsync_notifier_period = 0;//(1 * HZ);
	pinfo->bl_max = 7;
	pinfo->bl_min = 1;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);
//	else
//		mddi_lcd.vsync_detected = mddi_cci_kb60_lcd_vsync_detected;

	DEBUG("##### %s done#####\n",__func__);
	return ret;
}

module_init(mddi_cci_kb60_init);
