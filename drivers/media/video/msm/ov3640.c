/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include "ov3640.h"


#include <linux/device.h> /* for vreg.h */
#include <mach/vreg.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <mach/camera.h>

#if 0
#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
extern int DVT_2;
#endif
#else
extern int DVT_2;
#endif

int IS_DVT_2 = 0;
bool LENC = true;

#include <mach/msm_battery.h>//bincent

/* Micron ov3640 Registers and their values */
/* Sensor Core Registers */
#define  REG_OV3640_MODEL_ID 0x3000
#define  OV3640_MODEL_ID     0x1580

/*  SOC Registers Page 1  */
#define  REG_OV3640_SENSOR_RESET     0x301A
#define  REG_OV3640_STANDBY_CONTROL  0x3202
#define  REG_OV3640_MCU_BOOT         0x3386

#define GPIO_PWDN 0
#define GPIO_RESET 1
#define GPIO_MCLK 15
#define PV_CAPTURE_DEBUG	0
#define MAX_EXPOSURE_LINE	1568

#define  LOW_LIGHT_BOUNDARY							0x1f

#define  HIGH_LIGHT_UP_BOUNDARY_HIGH_BYTE			0x00
#define  HIGH_LIGHT_UP_BOUNDARY_LOW_BYTE			0x80
#define  HIGH_LIGHT_DOWN_BOUNDARY_HIGH_BYTE		0x00
#define  HIGH_LIGHT_DOWN_BOUNDARY_LOW_BYTE			0xf0

#define		CAMERA_PID_REG		0x30dd
#define		CAMERA_TYPE_R2C		0x00
#define		CAMERA_TYPE_R2D		0x02

int ov3640_version;

struct ov3640_work_t {
	struct work_struct work;
};

static struct  ov3640_work_t *ov3640_sensorw;
static struct wake_lock ov3640_wake_lock;//CH, 20100721, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
static struct  i2c_client *ov3640_client;

struct ov3640_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};

//CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
#define OV3640_POWER_OFF  0
#define OV3640_POWER_ON   1
// CH end 

static struct ov3640_ctrl *ov3640_ctrl;
//CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
static int ov3640_power_state = OV3640_POWER_OFF;
static int ov3640_resume_power_flag = 0;
// CH end 

static DECLARE_WAIT_QUEUE_HEAD(ov3640_wait_queue);
DECLARE_MUTEX(ov3640_sem);

/*=============================================================*/

static void ov3640_get_sensor_version(void);

void msm_camera_power_enable(void)
{
	int rc;
	struct vreg *vreg_cam_vcm , *vreg_cam_io , *vreg_cam_avdd, *vreg_cam_dcore;
	printk("#### %s ####\n", __FUNCTION__);

#if 0
	#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
		IS_DVT_2 = DVT_2;
	#endif
#else
IS_DVT_2 = DVT_2;
#endif

	vreg_cam_vcm = vreg_get(0, "gp6");  //VCM used
    	rc = vreg_set_level(vreg_cam_vcm,2800);
    	if (rc)
    		printk("#### vreg set gp6 level failed ####\n");
	
	//mdelay(5);

	if(IS_DVT_2) {
		 printk("#### vreg set gp2 level ####\n");
    		vreg_cam_io = vreg_get(0, "gp2"); //I/O used on DVT2
    		rc = vreg_set_level(vreg_cam_io,1800);
    
    		if (rc)
    			printk("#### vreg set gp2 level failed ####\n");
	}
	else {
		printk("#### vreg set rftx level ####\n");
		vreg_cam_io = vreg_get(0, "rftx");	//I/O used on DVT1
		//rc = vreg_set_level(vreg_cam,2600);
		rc = vreg_set_level(vreg_cam_io,1800);
		if (rc)
			printk("#### vreg set rftx level failed ####\n");
	}

	//mdelay(8);

    vreg_cam_avdd = vreg_get(0, "gp3"); //Analog used
    rc = vreg_set_level(vreg_cam_avdd,2800);
    if (rc)
    printk("#### vreg set gp3 level failed ####\n");
    //	internal LDO => external LDO
    vreg_cam_dcore=vreg_get(0, "rftx");	//VDD_D
    rc = vreg_set_level(vreg_cam_dcore,1500);
    if (rc)
    printk("#### vreg set rftx level failed ####\n");

	rc = vreg_enable(vreg_cam_vcm);
    	if (rc)
    		printk("#### vreg enable gp6 level failed ####\n");

	rc = vreg_enable(vreg_cam_io);
    	if (rc)
        printk("#### vreg enable gp2level failed ####\n");

	rc = vreg_enable(vreg_cam_avdd);
    if (rc)
    	printk("#### vreg enable gp3 level failed ####\n");

    //  internal LDO => external LDO
    rc = vreg_enable(vreg_cam_dcore);
    if (rc)
    printk("#### vreg enable rftx  level failed ####\n");


}

#if 0
void msm_camera_power_disable(void)
{
    int rc;
    struct vreg *vreg_cam;

    printk("#### %s ####\n", __FUNCTION__);

    #ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
     	    IS_DVT_2 = DVT_2;
    #endif

    vreg_cam = vreg_get(0, "gp3");	//AVDD
    rc = vreg_set_level(vreg_cam,0);
    if (rc)
    	printk("#### vreg set gp3 level failed ####\n");
    rc = vreg_disable(vreg_cam);
    if (rc)
    	printk("#### vreg disable gp3 level failed ####\n");
	
    if(IS_DVT_2) {
	vreg_cam = vreg_get(0, "gp2");	//I/O on DVT2
    	rc = vreg_set_level(vreg_cam,0);
    	if (rc)
    		printk("#### vreg set gp2 level failed ####\n");
    	rc = vreg_disable(vreg_cam);
    	if (rc)
    		printk("#### vreg disable gp2 level failed ####\n");

    }
    else {
    	vreg_cam = vreg_get(0, "rftx");	//I/O on DVT1
    	rc = vreg_set_level(vreg_cam,0);
    	if (rc)
    		printk("#### vreg set rftx level failed ####\n");
    	rc = vreg_disable(vreg_cam);
    	if (rc)
    		printk("#### vreg disable rftx level failed ####\n");
    }

    vreg_cam = vreg_get(0, "gp6");	//vcm
    rc = vreg_set_level(vreg_cam,0);
    if (rc)
    	printk("#### vreg set gp6 level failed ####\n");
    rc = vreg_disable(vreg_cam);
    if (rc)
    	printk("#### vreg disable gp6 level failed ####\n");

}
#endif

static int ov3640_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
	//CDBG("#### %s ####\n", __FUNCTION__);

	rc = gpio_request(dev->sensor_reset, "ov3640");

	if (!rc) {
		//CDBG("#### ov3640_reset ing ####\n");
		rc = gpio_direction_output(dev->sensor_reset, 1);
	}

	gpio_free(dev->sensor_reset);
	return rc;
}

static int ov3640_powerdown(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
	//CDBG("#### %s ####\n", __FUNCTION__);

	rc = gpio_request(dev->sensor_pwd, "ov3640");

	if (!rc) {
		//CDBG("#### ov3640_powerdown ing ####\n");
		//rc = gpio_direction_output(dev->sensor_pwd, 1);
		//mdelay(5);
		rc = gpio_direction_output(dev->sensor_pwd, 0);
	}

	gpio_free(dev->sensor_pwd);
	return rc;
}
static int ov3640_powerdownhigh(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
	//CDBG("#### %s ####\n", __FUNCTION__);

	rc = gpio_request(dev->sensor_pwd, "ov3640");

	if (!rc) {
		//CDBG("#### ov3640_powerdown ing ####\n");
		//rc = gpio_direction_output(dev->sensor_pwd, 1);
		//mdelay(5);
		rc = gpio_direction_output(dev->sensor_pwd, 1);
	}

	gpio_free(dev->sensor_pwd);
	return rc;
}


int ov3640_power_on_sequence(const struct msm_camera_sensor_info *data)
{
	int rc;
	struct msm_camera_device_platform_data *camdev = data->pdata;

	ov3640_powerdownhigh(data);
	mdelay(5);
	msm_camera_power_enable();  //VCM -> I/O -> AVDD

	mdelay(10);
	rc = ov3640_powerdown(data);
	if (rc < 0) {
	  	printk("powerdown failed!\n");
	  	return rc;
	}
	mdelay(10);

	rc = ov3640_reset(data);
	if (rc < 0) {
	  	printk("reset failed!\n");
	  	return rc;
	}

	mdelay(10);

	camdev->camera_gpio_on();//adjust gpio sequence

	/* Input MCLK = 24MHz */
	//msm_camio_clk_enable(CAMIO_MDC_CLK);
	//msm_camio_clk_enable(CAMIO_VFE_MDC_CLK);
	msm_camio_clk_enable(CAMIO_VFE_CLK);//adjust MCLK sequence
	msm_camio_clk_rate_set(24000000);//set MCLK 24M HZ

        //[KB62] Merge BSP5330 battery function, weijiun_ling
        #ifdef NOTIFY_BATT__FUNCTION
            cci_batt_device_status_update(CCI_BATT_DEVICE_ON_CAMERA_150,1);
        #endif
        //End

//CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
  ov3640_power_state = OV3640_POWER_ON;
// CH end 

	return rc;
}    

void ov3640_power_off_sequence(void)
{
    int rc;
    struct vreg *vreg_cam_vcm , *vreg_cam_io , *vreg_cam_avdd,*vreg_cam_dcore;

    printk("#### %s ####\n", __FUNCTION__);

#if 0
    #ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
     	    IS_DVT_2 = DVT_2;
    #endif
#else
IS_DVT_2 = DVT_2;
#endif

    vreg_cam_avdd = vreg_get(0, "gp3");	//AVDD
    rc = vreg_set_level(vreg_cam_avdd,0);

     if(IS_DVT_2) {
		vreg_cam_io = vreg_get(0, "gp2");	//I/O on DVT2
     		rc = vreg_set_level(vreg_cam_io,0);
     		if (rc)
    			printk("#### vreg set gp2 level failed ####\n");
     }
     else {
    		vreg_cam_io = vreg_get(0, "rftx");	//I/O on DVT1
    		rc = vreg_set_level(vreg_cam_io,0);
    		if (rc)
    			printk("#### vreg set rftx level failed ####\n");
    }

    //  internal LDO => external LDO
    vreg_cam_dcore= vreg_get(0, "rftx");
    rc = vreg_set_level(vreg_cam_dcore,0);
    if (rc)
    printk("#### vreg set rftx level failed ####\n");

    vreg_cam_vcm = vreg_get(0, "gp6");	//vcm
    rc = vreg_set_level(vreg_cam_vcm,0);
    if (rc)
    printk("#### vreg set gp6 level failed ####\n");

    //  internal LDO => external LDO
     rc = vreg_disable(vreg_cam_dcore);
    if (rc)
    printk("#### vreg disable vreg_cam_dcore level failed ####\n");

     rc = vreg_disable(vreg_cam_avdd);
    if (rc)
    	printk("#### vreg disable gp3 level failed ####\n");

     rc = vreg_disable(vreg_cam_io);
    if (rc)
    		printk("#### vreg disable gp2 level failed ####\n");

     mdelay(1);

     gpio_set_value(GPIO_RESET,0);

     msm_camio_clk_disable(CAMIO_VFE_CLK);//adjust MCLK sequence

    rc = vreg_disable(vreg_cam_vcm);
    if (rc)
    	printk("#### vreg disable gp6 level failed ####\n");

    //[KB62] Merge BSP5330 battery function, weijiun_ling
    #ifdef NOTIFY_BATT__FUNCTION
        cci_batt_device_status_update(CCI_BATT_DEVICE_ON_CAMERA_150,0);
    #endif
    //End

//CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
  ov3640_power_state = OV3640_POWER_OFF;
// CH end   

}


int msm_camera_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("#### %s ####\n", __FUNCTION__);
/*
	if(gpio_get_value(GPIO_RESET)){
		printk("ov3640 : Suspend in AP running .\n");
		ov3640_power_off_sequence();
		}
	else{
		printk("ov3640 : Suspend in AP not running .\n");
		msm_camera_power_disable();
		}
*/
        //CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
	if(ov3640_power_state == OV3640_POWER_ON)
        {
		printk("ov3640 : Suspend Camera Sensor .\n");
		ov3640_power_off_sequence();
                ov3640_resume_power_flag = 1;
	}
        // CH end 
	return 0;
}

//CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
static int ov3640_sensor_init_probe(const struct msm_camera_sensor_info *data);
// CH end 

int msm_camera_resume(struct platform_device *pdev)
{
	printk("#### %s ####\n", __FUNCTION__);
/*
	if(gpio_get_value(GPIO_RESET)){
		printk("ov3640 : Resume in AP running .\n");
		ov3640_power_on_sequence(pdev->dev.platform_data);
		}
	else{
		printk("ov3640 : Resume in AP not running .\n");
		msm_camera_power_enable();
		}
*/

        //CH, 20100719, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
	if(ov3640_resume_power_flag)
        {
		printk("ov3640 : Resume Camera Sensor .\n");
		ov3640_power_on_sequence(pdev->dev.platform_data);
                ov3640_sensor_init_probe(pdev->dev.platform_data);
                ov3640_resume_power_flag = 0;
	}
        // CH end 
	return 0;
}

static int32_t ov3640_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
#if 1
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};
#endif
	unsigned char i=0;
	
	//CDBG("#### %s ####\n", __FUNCTION__);

	for (i=0; i<5; i++)
	{
		if (i2c_transfer(ov3640_client->adapter, msg, 1) >= 0) 
		//if (i2c_master_send(ov3640_client, txdata, 1) >= 0) 
			return 0;
		mdelay(10);
	}
	printk("ov3640_i2c_txdata faild\n");
	return -EIO;
}

static int32_t ov3640_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum ov3640_width_t width)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	//CDBG("#### %s ####\n", __FUNCTION__);

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

		rc = ov3640_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		#if 0
		buf[0] = waddr;
		buf[1] = wdata;
		#else
		buf[0] = (unsigned char)((waddr & 0xFF00)>>8);
		buf[1] = (unsigned char)(waddr & 0x00FF);
		buf[2] = (unsigned char)(wdata);
		#endif
		rc = ov3640_i2c_txdata(saddr, buf, 3);
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		printk(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}
#if 1
static int ov3640_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};
	unsigned char i=0;
	//CDBG("#### %s ####\n", __FUNCTION__);

	for (i=0; i<5; i++)
	{
		if (i2c_transfer(ov3640_client->adapter, msgs, 2) >= 0) {
			return 0;
		mdelay(10);			
		}
	}
	printk("ov3640_i2c_rxdata faild\n");
	return -EIO;
}

static int32_t ov3640_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum ov3640_width_t width)
{
	int32_t rc = 0;
	unsigned char buf[4];
	//CDBG("#### %s ####\n", __FUNCTION__);

	if (!rdata)
	{
		printk("#### No rdata Error !!! ####\n");
		return -EIO;
	}

	memset(buf, 0, sizeof(buf));

	switch (width) {
	case WORD_LEN: {
		//CDBG("#### ov3640_i2c_read WORD_LEN !!! ####\n");
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);

		rc = ov3640_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			{
			printk("i2c_read failed, raddr = 0x%x!\n", raddr);
			return rc;
			}

		//*rdata = buf[0] << 8 | buf[1];
		*rdata = buf[0];
		#if PV_CAPTURE_DEBUG
		printk("#### I2c_read Addr = 0x%x, Data = 0x%x ####\n", raddr, *rdata);
		#endif
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		printk("ov3640_i2c_read failed!\n");

	return rc;
}
#endif
static void ov3640_get_sensor_version(void)
{
	long rc = 0;
	unsigned short Temp=0;

	printk("#### %s ####\n", __FUNCTION__);

	rc = ov3640_i2c_read(ov3640_client->addr, CAMERA_PID_REG,&Temp, WORD_LEN);
	if (rc < 0)
	{
		printk("error to read sensor version.\n");
		return;
	}
	printk("sensor version = 0x%x.\n", Temp);

	ov3640_version = Temp;
}

#if 0

static int32_t ov3640_i2c_write_mask(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum ov3640_width_t width, unsigned char mask)
{
	int32_t rc = -EFAULT;
	unsigned short Temp=0;
	
	//printk("#### %s ####\n", __FUNCTION__);

	rc = ov3640_i2c_read(saddr,waddr,&Temp, WORD_LEN);
	if (rc < 0)
		goto i2c_write_mask_fail;
	//printk("#### ov3640_i2c_write_mask read Temp  0x%x ####\n", Temp);
	Temp=(((unsigned char)Temp)&(~mask))|(unsigned char)wdata;
	
	rc = ov3640_i2c_write(saddr,waddr,(unsigned short)Temp, BYTE_LEN); 
	if (rc < 0)
		goto i2c_write_mask_fail;

	return rc;
	
i2c_write_mask_fail:
	printk("#### ov3640_i2c_write_mask fail ####\n");
	return rc;
}
#endif

#if 0

static int32_t ov3640_i2c_write_table(
	struct ov3640_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EFAULT;
	CDBG("#### %s ####\n", __FUNCTION__);

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			mdelay(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

static int32_t ov3640_set_lens_roll_off(void)
{
	int32_t rc = 0;
	CDBG("#### %s ####\n", __FUNCTION__);
	rc = ov3640_i2c_write_table(&ov3640_regs.rftbl[0],
		ov3640_regs.rftbl_size);
	return rc;
}

static long ov3640_reg_init(void)
{
	int32_t array_length;
	int32_t i;
	long rc;
	CDBG("#### %s ####\n", __FUNCTION__);

	/* PLL Setup Start */
	rc = ov3640_i2c_write_table(&ov3640_regs.plltbl[0],
		ov3640_regs.plltbl_size);

	if (rc < 0)
		return rc;
	/* PLL Setup End   */

	array_length = ov3640_regs.prev_snap_reg_settings_size;


	/* Configure sensor for Preview mode and Snapshot mode */
	for (i = 0; i < array_length; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.prev_snap_reg_settings[i].register_address,
		  ov3640_regs.prev_snap_reg_settings[i].register_value,
		  WORD_LEN);

		if (rc < 0)
			return rc;
	}

	/* Configure for Noise Reduction, Saturation and Aperture Correction */
	array_length = ov3640_regs.noise_reduction_reg_settings_size;

	for (i = 0; i < array_length; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
		ov3640_regs.noise_reduction_reg_settings[i].register_address,
		ov3640_regs.noise_reduction_reg_settings[i].register_value,
		WORD_LEN);

		if (rc < 0)
			return rc;
	}

	/* Set Color Kill Saturation point to optimum value */
	rc =
	ov3640_i2c_write(ov3640_client->addr,
	0x35A4,
	0x0593,
	WORD_LEN);
	if (rc < 0)
		return rc;

	rc = ov3640_i2c_write_table(&ov3640_regs.stbl[0],
		ov3640_regs.stbl_size);
	if (rc < 0)
		return rc;

	rc = ov3640_set_lens_roll_off();
	if (rc < 0)
		return rc;

	return 0;
}
#endif

static long ov3640_set_capture_delay(void)
{
	unsigned short Temp=0;	
	unsigned short Multiple=1;	
	unsigned short Time=330;
	int rc = 0;

	rc = ov3640_i2c_read(ov3640_client->addr, 0x300e,&Temp, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_capture_delay_fail;

	printk("#### ov3640_set_capture_delay : Night Mode 0x%x ####\n", Temp);

	if (Temp==0x39)
		Multiple=2;
	
	rc = ov3640_i2c_read(ov3640_client->addr, 0x302d,&Temp, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_capture_delay_fail;	

	switch(Temp)
	{
		case 0:
//CH, 20100726, Bug 140 - [BSP][Camera]Sync KB60 to KB62 improve the camera response
			Time=75;
		break;
		case 2:
			Time=150;
		break;
		case 4:
			Time=200;
		break;		
		default :
			Time=75;
//CH, end
		break;	
	}

	printk("#### ov3640_set_capture_delay : %d ####\n", (Time*Multiple));
	mdelay((Time*Multiple));
	
	return rc;
	ov3640_set_capture_delay_fail:
	printk("#### ov3640_set_capture_delay fail ####\n");
	return rc;
}

static long ov3640_set_exposure_line(void)
{
	unsigned short ELHighByte=0;	
	unsigned short ELLowByte=0;	
	unsigned short DLHighByte=0;	
	unsigned short DLLowByte=0;	
	unsigned short ExposureLine=0;	
	unsigned short DummyLine=0; 
	unsigned short ExtraLine=0; 
	
	int rc = 0;

	rc = ov3640_i2c_read(ov3640_client->addr, 0x3002,&ELHighByte, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_exposure_line_fail;
	rc = ov3640_i2c_read(ov3640_client->addr, 0x3003,&ELLowByte, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_exposure_line_fail;
	rc = ov3640_i2c_read(ov3640_client->addr, 0x302d,&DLHighByte, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_exposure_line_fail;
	rc = ov3640_i2c_read(ov3640_client->addr, 0x302e,&DLLowByte, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_exposure_line_fail;

	ExposureLine = ELHighByte<<8 | ELLowByte;
	DummyLine = DLHighByte<<8 | DLLowByte;
	

	if ((ExposureLine < MAX_EXPOSURE_LINE) && (DummyLine>0))
	{
		ExtraLine = MAX_EXPOSURE_LINE - ExposureLine; 
		if(DummyLine> ExtraLine)
		{
			ExposureLine = MAX_EXPOSURE_LINE;
			DummyLine -= ExtraLine;
		}
		else
		{
			ExposureLine += DummyLine;
			DummyLine = 0;
		}	

		ELHighByte = ExposureLine>>8;
		ELLowByte = ExposureLine & 0x00FF;
		DLHighByte = DummyLine >>8;
		DLLowByte = DummyLine & 0x00FF;

		rc = ov3640_i2c_write(ov3640_client->addr, 0x3002,  ELHighByte, BYTE_LEN);
		if (rc < 0)
			goto ov3640_set_exposure_line_fail;
		rc = ov3640_i2c_write(ov3640_client->addr, 0x3003,  ELLowByte, BYTE_LEN);
		if (rc < 0)
			goto ov3640_set_exposure_line_fail;
		rc = ov3640_i2c_write(ov3640_client->addr, 0x302d,  DLHighByte, BYTE_LEN);
		if (rc < 0)
			goto ov3640_set_exposure_line_fail;
		rc = ov3640_i2c_write(ov3640_client->addr, 0x302e,  DLLowByte, BYTE_LEN);
		if (rc < 0)
			goto ov3640_set_exposure_line_fail;
	}
	return rc;
	
	ov3640_set_exposure_line_fail:
	printk("#### ov3640_set_capture_delay fail ####\n");
	return rc;
}


static long ov3640_video_config(int mode)
{

	int rc = 0;	

	int32_t i=0;
	int32_t array_length=0;

	printk("#### %s ####\n", __FUNCTION__);
	
	array_length = ov3640_regs.PV_reg_settings_size;
	printk("OV3640 PV register setting start\n");
	for (i = 0; i < array_length; i++) {
			rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.PV_reg_settings[i].register_address,
		  ov3640_regs.PV_reg_settings[i].register_value,
		  BYTE_LEN);

		if (rc < 0)
			goto video_config_fail;
		udelay(10);
	}
	printk("OV3640 PV register setting end\n");
	return rc;
	
video_config_fail:
	printk("#### ov3640_video_config fail ####\n");

	return rc;
}
static long ov3640_movie_config(int mode)
{

	int rc = 0;	

	int32_t i=0;
	int32_t array_length=0;

	printk("#### %s ####\n", __FUNCTION__);
	
	array_length = ov3640_regs.MV_reg_settings_size;
	printk("OV3640 MV register setting start\n");
	for (i = 0; i < array_length; i++) {
			rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.MV_reg_settings[i].register_address,
		  ov3640_regs.MV_reg_settings[i].register_value,
		  BYTE_LEN);

		if (rc < 0)
			goto movie_config_fail;
		udelay(10);
	}
	printk("OV3640 MV register setting end\n");
	return rc;
	
movie_config_fail:
	printk("#### ov3640_movie_config fail ####\n");

	return rc;
}

static long ov3640_snapshot_config(int mode)
{
	int rc = 0;
	int32_t i=0;
	int32_t array_length=0;	
#if PV_CAPTURE_DEBUG	
	unsigned short Temp=0;	
#endif

	printk("#### %s ####\n", __FUNCTION__);
	
#if PV_CAPTURE_DEBUG
		printk("OV3640 register debug before capture\n");
	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x301b,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3012,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3014,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3015,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3000,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3001,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3002,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3003,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x302d,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x302e,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3366,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3302,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x300e,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x300f,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3011,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x304c,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
#endif

	array_length = ov3640_regs.CP_reg_settings_size;
	printk("OV3640 CP register setting start\n");
	for (i = 0; i < array_length; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.CP_reg_settings[i].register_address,
		  ov3640_regs.CP_reg_settings[i].register_value,
		  BYTE_LEN);

		if (rc < 0)
			goto snapshot_config_fail;
		//udelay(10);
		}
	printk("OV3640 CP register setting end\n");

	ov3640_set_exposure_line();
	ov3640_set_capture_delay();
	
#if PV_CAPTURE_DEBUG
		printk("OV3640 register debug after capture\n");
	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x301b,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3012,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3014,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3015,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3000,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3001,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3002,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3003,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x302d,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x302e,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3366,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;	
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3302,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x300e,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x300f,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3011,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
		rc = ov3640_i2c_read(ov3640_client->addr, 0x304c,&Temp, WORD_LEN);
		if (rc < 0)
			goto snapshot_config_fail;		
#endif
	return rc;
snapshot_config_fail:
	printk("#### ov3640_snapshot_config fail ####\n");
	return rc;

	
}
static long ov3640_raw_snapshot_config(int mode)
{
	int rc = 0;
	
	printk("#### %s ####\n", __FUNCTION__);

	return rc;
}


static long ov3640_set_sensor_mode(int mode)
{
	long rc = 0;
	printk("#### %s ####\n", __FUNCTION__);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = ov3640_video_config(mode);
		break;

	case SENSOR_SNAPSHOT_MODE:
		rc = ov3640_snapshot_config(mode);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = ov3640_raw_snapshot_config(mode);
		break;
	case SENSOR_VIDEO_MODE:
			rc = ov3640_movie_config(mode);
			break;

	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static long ov3640_set_effect(
  int mode,
  int8_t effect
)
{
  long rc = 0;
  unsigned short Temp = 0;
  printk("#### %s ####\n", __FUNCTION__);
  rc = ov3640_i2c_read(ov3640_client->addr, 0x3355, &Temp, WORD_LEN);
  if (rc < 0)
    goto ov3640_set_effect_fail;

  switch (mode) {
    case SENSOR_PREVIEW_MODE:
    case SENSOR_SNAPSHOT_MODE:
    case SENSOR_RAW_SNAPSHOT_MODE:
    case SENSOR_VIDEO_MODE:
      printk("#### ov3640_set_effect %d ####\n", effect);

      switch (effect)
      {
        case CAMERA_EFFECT_MONO:
          Temp &= (~0x20);
          Temp &= (~0x40);
          Temp |= 0x18;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN);
          if (rc < 0)
          goto ov3640_set_effect_fail;
              rc = ov3640_i2c_write(ov3640_client->addr,0x335a,0x80, BYTE_LEN); 
          if (rc < 0)
              goto ov3640_set_effect_fail;
          rc = ov3640_i2c_write(ov3640_client->addr,0x335b,0x80, BYTE_LEN); 
          if (rc < 0)
              goto ov3640_set_effect_fail;
          break;
        case CAMERA_EFFECT_NEGATIVE:
          Temp &= (~0x20);
          Temp |= 0x44;
          Temp &= (~0x18);
          rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN);
          if (rc < 0)
            goto ov3640_set_effect_fail;
          break;
        case CAMERA_EFFECT_SEPIA:
          Temp &= (~0x20);
          Temp &= (~0x40);
          Temp |= 0x18;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN);
          if (rc < 0)
            goto ov3640_set_effect_fail;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x335a, 0x60, BYTE_LEN);
          if (rc < 0)
            goto ov3640_set_effect_fail;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x335b, 0x9c, BYTE_LEN);//ori is 0x8c
          if (rc < 0)
            goto ov3640_set_effect_fail;
          break;	
        case CAMERA_EFFECT_AQUA:
          Temp &= (~0x20);
          Temp &= (~0x40);
          Temp |= 0x18;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
          if (rc < 0)
            goto ov3640_set_effect_fail;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x335a, 0xa0, BYTE_LEN); 
          if (rc < 0)
            goto ov3640_set_effect_fail;
          rc = ov3640_i2c_write(ov3640_client->addr, 0x335b, 0x40, BYTE_LEN); 
          if (rc < 0)
            goto ov3640_set_effect_fail;
          break;
        case CAMERA_EFFECT_POSTERIZE:
        case CAMERA_EFFECT_SOLARIZE:
        case CAMERA_EFFECT_WHITEBOARD:
        case CAMERA_EFFECT_BLACKBOARD:
        case CAMERA_EFFECT_OFF:
        default:
          Temp &= (~0x20);
          Temp &= (~0x40);
          Temp &= (~0x18);
          rc = ov3640_i2c_write(ov3640_client->addr,0x335a,0x80, BYTE_LEN); 
          rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
          if (rc < 0)
            goto ov3640_set_effect_fail;
          break;
      }
      break;
    }
    return rc;
  ov3640_set_effect_fail:
    printk("#### ov3640_set_effect_fail ####\n");
    return rc;
}

static long ov3640_set_wb(
	int mode,
	int8_t wb
)
{
	long rc = 0;
	printk("#### %s ####\n", __FUNCTION__);

	switch (mode) {
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE:		
		printk("#### ov3640_set_wb %d ####\n", wb);
		switch (wb)
		{
			case CAMSENSOR_WB_INCANDESCENT:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x64, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_FLUORESCENT:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x52, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x5a, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_DAYLIGHT:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x5a, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x48, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_TWILIGHT:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x3c, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x6f, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_SHADE:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x5a, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x4c, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x48, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_CLOUDY_DAYLIGHT:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x08, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a7,0x68, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a8,0x40, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x33a9,0x50, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
			case CAMSENSOR_WB_AUTO:
			default:
				rc = ov3640_i2c_write(ov3640_client->addr,0x332b,0x00, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_wb_fail;
			break;
		}
		break;
	}
	return rc;
ov3640_set_wb_fail :
	printk("#### ov3640_set_wb_fail ####\n");
	return rc;
}

static long ov3640_set_brightness(
	int mode,
	int8_t brightness
)
{
	long rc = 0;
	//unsigned short Temp=0;
	printk("#### %s ####\n", __FUNCTION__);

	switch (mode) {
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

		printk("#### ov3640_set_brightness %d ####\n", brightness);
		
		switch (brightness)
		{
			case BRIGHTNESS_NEGATIVE_20:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x14, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x0c, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0x31, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;
			break;
			case BRIGHTNESS_NEGATIVE_13:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x20, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x18, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0x41, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;
			break;
			case BRIGHTNESS_NEGATIVE_7:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x2c, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x24, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0x51, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;			
			break;
			case BRIGHTNESS_POSITIVE_7:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x58, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x50, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0x81, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;	
			break;
			case BRIGHTNESS_POSITIVE_13:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x78, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x70, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0xa1, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;	
			break;
			case BRIGHTNESS_POSITIVE_20:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x98, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x90, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0xb1, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;
			break;
			case BRIGHTNESS_DEFAULT:  
			default:
				rc =ov3640_i2c_write(ov3640_client->addr,0x3018, 0x38, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc =ov3640_i2c_write(ov3640_client->addr,0x3019, 0x30, BYTE_LEN);
				if (rc < 0)
					goto ov3640_set_brightness_fail;
				rc = ov3640_i2c_write(ov3640_client->addr,0x301a, 0x61, BYTE_LEN); 
				if (rc < 0)
					goto ov3640_set_brightness_fail;
			break;
		}
		break;
	}
	return rc;
ov3640_set_brightness_fail :
	printk("#### ov3640_set_brightness_fail ####\n");
	return rc;
}

static long ov3640_set_antibanding(
	int mode,
	int8_t antibanding
)
{
	long rc = 0;
	unsigned short Temp=0;

	
	printk("#### %s ####\n", __FUNCTION__);

	switch (mode) {
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

			printk("#### ov3640_set_antibanding %d ####\n", antibanding);

			switch (antibanding)
		  	{
				case CAMSENSOR_ANTIBANDING_50HZ:
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp |= 0x20;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3013,(unsigned short)Temp, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3014,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp |= 0x80;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3014,(unsigned short)Temp, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					break;
				case CAMSENSOR_ANTIBANDING_60HZ:
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp |= 0x20;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3013,(unsigned short)Temp, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3014,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp &= 0x7f;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3014,(unsigned short)Temp, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					break;
		  		case CAMSENSOR_ANTIBANDING_OFF:
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp &= 0xdf;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3013,(unsigned short)Temp, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					break;
				case CAMSENSOR_ANTIBANDING_AUTO:
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp |= 0x20;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3013,(unsigned short)Temp, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3014,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp |= 0x40;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3014,(unsigned short)Temp, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					break;
				default:
					rc = ov3640_i2c_read(ov3640_client->addr, 0x3013,&Temp, WORD_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					Temp &= 0xdf;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3013,(unsigned short)Temp, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_antibanding_fail;
					break;
		  	}
			break;
		}
		return rc;
ov3640_set_antibanding_fail :
	printk("#### ov3640_set_antibanding_fail ####\n");
	return rc;
}
static long ov3640_set_iso(int mode,	int8_t brightness)
{
	int rc = 0;
	
	printk("#### %s ####\n", __FUNCTION__);

	return rc;
}
static long ov3640_set_focus_mode(int mode,	int8_t focus_mode)
{
	int rc = 0;	
	//int i=0;
	unsigned short Temp=0x00;	

	printk("#### %s ####\n", __FUNCTION__);

	rc = ov3640_i2c_read(ov3640_client->addr, 0x300e,&Temp, WORD_LEN);
	if (rc < 0)
		goto ov3640_set_focus_mode_fail;

	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

			printk("#### ov3640_set_focus_mode %d ####\n", focus_mode);

			switch(focus_mode)
			{
				case CAMSENSOR_FOCUS_MODE_NORMAL:
					rc =ov3640_i2c_write(ov3640_client->addr,0x3f00,0x08, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
				break;	
				case CAMSENSOR_FOCUS_MODE_MACRO:
					rc =ov3640_i2c_write(ov3640_client->addr,0x3f00,0x15, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
				break;
				case CAMSENSOR_FOCUS_MODE_AUTO:
					rc = ov3640_i2c_write(ov3640_client->addr,0x3F00,0x08, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
					mdelay(5);
					rc =ov3640_i2c_write(ov3640_client->addr,0x3f00,0x05, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
				break;
				default :
					rc = ov3640_i2c_write(ov3640_client->addr,0x3F00,0x08, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
					mdelay(5);
					rc =ov3640_i2c_write(ov3640_client->addr,0x3f00,0x05, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_focus_mode_fail;
				break;
			}
		break;
	}
	return rc;
	
ov3640_set_focus_mode_fail :
	printk("#### ov3640_set_focus_mode_fail ####\n");
	return rc;
}
static long ov3640_set_exposure_mode(int mode,	int8_t ae_mode)
{
	int rc = 0;
	
	printk("#### %s ####\n", __FUNCTION__);

	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

			printk("#### ov3640_set_exposure_mode %d ####\n", ae_mode);

			switch(ae_mode)
			{
				case CAMSENSOR_AE_MODE_AVERAGE :
					rc =ov3640_i2c_write(ov3640_client->addr,0x3030,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3031,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3032,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3033,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3034,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3035,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3036,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3037,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;	
				break;	
				case CAMSENSOR_AE_MODE_CENTER :
					rc =ov3640_i2c_write(ov3640_client->addr,0x3030,0x62, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3031,0x26, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3032,0xe6, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3033,0x6e, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3034,0xea, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3035,0xae, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3036,0xa6, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3037,0x6a, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
				break;
				case CAMSENSOR_AE_MODE_SPOT :
					rc =ov3640_i2c_write(ov3640_client->addr,0x3030,0x00, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3031,0x00, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3032,0xa0, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3033,0x0a, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3034,0xa0, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3035,0x0a, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3036,0x00, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3037,0x00, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
				break;
				default :
					rc =ov3640_i2c_write(ov3640_client->addr,0x3030,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3031,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3032,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3033,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3034,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3035,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3036,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3037,0x11, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_exposure_mode_fail;
				break;
			}
		break;
	}
	return rc;
	
ov3640_set_exposure_mode_fail :
	printk("#### ov3640_set_exposure_mode_fail ####\n");
	return rc;
}
static long ov3640_set_lens_shading_correction(int mode,	int8_t lens_shading)
{
	int rc = 0;
	
	printk("#### %s ####\n", __FUNCTION__);
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

			printk("#### ov3640_set_lens_shading_correction %d ####\n", lens_shading);

			switch(lens_shading)
			{
				case CAMSENSOR_LENS_CORRECTION_DISABLE:
					LENC = false;
					rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x01, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x01, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x01, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
				break;	
				case CAMSENSOR_LENS_CORRECTION_ENABLE:
					LENC = true;
					rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x2a, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x24, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x21, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
				break;
				default :
					LENC = true;
					rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x2a, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x24, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x21, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_lens_shading_correction_fail;
				break;
			}
		break;
	}
	return rc;
	
ov3640_set_lens_shading_correction_fail :
	printk("#### ov3640_set_lens_shading_correction_fail ####\n");
	return rc;
}

static long ov3640_set_night_mode(int mode,	int8_t night_mode)
{
	int rc = 0;
	//unsigned short Temp=0;
	
	printk("#### %s ####\n", __FUNCTION__);
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE: 

			printk("#### ov3640_set_night_mode %d ####\n", night_mode);

			switch(night_mode)
			{
				case CAMSENSOR_NIGHT_MODE_DISABLE:
					rc = ov3640_i2c_write(ov3640_client->addr,0x3604,0x30, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3f00,0x16, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x300e,0x32, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3070,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3071,0xec, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3072,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3073,0xc4, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301c,0x02, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301d,0x02, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
				break;	
				case CAMSENSOR_NIGHT_MODE_ENABLE:
					rc = ov3640_i2c_write(ov3640_client->addr,0x3604,0x15, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3f00,0x16, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x300e,0x39, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3070,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3071,0x76, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3072,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3073,0x62, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301c,0x05, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301d,0x06, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
				break;
				default :
					rc = ov3640_i2c_write(ov3640_client->addr,0x3604,0x30, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3f00,0x16, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;

					rc = ov3640_i2c_write(ov3640_client->addr,0x300e,0x32, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3070,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3071,0xec, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3072,0x00, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x3073,0xc4, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301c,0x02, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
					rc = ov3640_i2c_write(ov3640_client->addr,0x301d,0x02, BYTE_LEN); 
					if (rc < 0)
						goto ov3640_set_night_mode_fail;
				break;
			}
		break;
	}
	return rc;

	ov3640_set_night_mode_fail :
	printk("#### ov3640_set_night_mode_fail ####\n");

	return rc;
}

static int32_t ov3640_move_focus(int direction,
	int32_t num_steps)
{
	int rc = 0;
	printk("#### %s ####\n", __FUNCTION__);

#if 1
	rc = ov3640_i2c_write(ov3640_client->addr,0x3F00,0x08, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_move_focus_fail;
	//mdelay(5);
	#if 0
	rc = ov3640_i2c_write(ov3640_client->addr,0x3F00,0x01, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_move_focus_fail;
	rc = ov3640_i2c_write(ov3640_client->addr,0x3F00,0x05, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_move_focus_fail;		
	#endif

	return rc;
	
ov3640_move_focus_fail :
	printk("#### ov3640_move_focus_fail ####\n");
#endif	
	return rc;
}

static int32_t ov3640_set_zoom_ae(int mode,	int8_t zoom)
{
	int rc = 0;
	uint16_t	x_MSB=0;
	uint16_t	x_LSB=0;
	uint16_t	y_MSB=0;
	uint16_t	y_LSB=0;
	uint16_t	w_MSB=0;
	uint16_t	w_LSB=0;
	uint16_t	h_MSB=0;
	uint16_t	h_LSB=0;	
	
	//printk("#### %s ####\n", __FUNCTION__);

	x_MSB = (ov3640_zoom_ae.zoom_ae_settings[zoom].start_x & 0xFF00)>>8;
	x_LSB = ov3640_zoom_ae.zoom_ae_settings[zoom].start_x & 0x00FF;
	y_MSB = (ov3640_zoom_ae.zoom_ae_settings[zoom].start_y & 0xFF00)>>8;
	y_LSB = ov3640_zoom_ae.zoom_ae_settings[zoom].start_y & 0x00FF;
	w_MSB = (ov3640_zoom_ae.zoom_ae_settings[zoom].size_x & 0xFF00)>>8;
	w_LSB = ov3640_zoom_ae.zoom_ae_settings[zoom].size_x & 0x00FF;
	h_MSB = (ov3640_zoom_ae.zoom_ae_settings[zoom].size_y & 0xFF00)>>8;
	h_LSB = ov3640_zoom_ae.zoom_ae_settings[zoom].size_y & 0x00FF;


	rc = ov3640_i2c_write(ov3640_client->addr,0x3038,x_MSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x3039,x_LSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303a,y_MSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303b,y_LSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303c,w_MSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303d,w_LSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303e,h_MSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	rc = ov3640_i2c_write(ov3640_client->addr,0x303f,h_LSB, BYTE_LEN); 
	if (rc < 0)
		goto ov3640_set_zoom_ae_fail;		
	
	return rc;

ov3640_set_zoom_ae_fail :
	printk("#### ov3640_set_zoom_ae_fail ####\n");
	return rc;
}

static int32_t ov3640_get_ae_status(int8_t *sdata)
{
	int rc = 0;
	unsigned short Temp=0;
	unsigned short ExpMLB=0;
	unsigned short ExpSLB=0;

	//printk("#### %s ####\n", __FUNCTION__);

		rc = ov3640_i2c_read(ov3640_client->addr, 0x3002,&ExpMLB, WORD_LEN);
		if (rc < 0)
			goto ov3640_get_ae_status_fail;
		rc = ov3640_i2c_read(ov3640_client->addr, 0x3003,&ExpSLB, WORD_LEN);
		if (rc < 0)
			goto ov3640_get_ae_status_fail;			

		if ((ExpMLB==HIGH_LIGHT_UP_BOUNDARY_HIGH_BYTE)&&(ExpSLB<=HIGH_LIGHT_UP_BOUNDARY_LOW_BYTE))
		{
			*sdata = AE_STATUS_HIGH_LIGHT;
		}
		else if ((ExpMLB>HIGH_LIGHT_DOWN_BOUNDARY_HIGH_BYTE)||(ExpSLB>=HIGH_LIGHT_DOWN_BOUNDARY_LOW_BYTE))
		{
			rc = ov3640_i2c_read(ov3640_client->addr, 0x3001,&Temp, WORD_LEN);
			if (rc < 0)
				goto ov3640_get_ae_status_fail; 			
			if (Temp >= LOW_LIGHT_BOUNDARY)
			{
				*sdata = AE_STATUS_LOW_LUX;
				goto ov3640_get_ae_status_done;
			}
			else
			{
				*sdata = AE_STATUS_NORMAL;
				goto ov3640_get_ae_status_done;
			}
				
		}
		//else if (*sdata == AE_STATUS_LOW_LUX)//CH Remove
		//	*sdata = AE_STATUS_NORMAL;
		
ov3640_get_ae_status_done:
	//printk("#### AE Status Result = %d####\n", *sdata);
	return rc;
	
ov3640_get_ae_status_fail :
	printk("#### ov3640_get_ae_status fail####\n");
	return rc;
}

static long ov3640_set_ae_gain(int mode,	int8_t gain)
{
	int rc = 0;	
	//printk("#### %s ####\n", __FUNCTION__);
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:	
		case SENSOR_VIDEO_MODE:				
			//printk("#### ov3640_set_ae_gain %d ####\n", gain);
			switch(gain)
			{
				case CAMSENSOR_AE_GAIN_NORMAL:
				default :
					rc =ov3640_i2c_write(ov3640_client->addr,0x3358,0x40, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3359,0x40, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x335d,0x20, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x332f,0x06, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3331,0x02, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					if (LENC)
					{
						rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x2a, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x24, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x21, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
					}
				break;	
				case CAMSENSOR_AE_GAIN_LOW_LUX:
					rc =ov3640_i2c_write(ov3640_client->addr,0x3358,0x30, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3359,0x30, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x335d,0x20, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x332f,0x03, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3331,0x10, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					if (LENC)
					{
						rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x1a, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x14, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x11, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
					}	
				break;
				case CAMSENSOR_AE_GAIN_HIGH_LIGHT:
					rc =ov3640_i2c_write(ov3640_client->addr,0x3358,0x48, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3359,0x50, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x335d,0x1c, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x332f,0x06, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					rc =ov3640_i2c_write(ov3640_client->addr,0x3331,0x02, BYTE_LEN);
					if (rc < 0)
						goto ov3640_set_ae_gain_fail;
					if (LENC)
					{
						rc =ov3640_i2c_write(ov3640_client->addr,0x336a,0x2a, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3370,0x24, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
						rc =ov3640_i2c_write(ov3640_client->addr,0x3376,0x21, BYTE_LEN);
						if (rc < 0)
							goto ov3640_set_ae_gain_fail;
					}
				break;
			}
		break;
	}
	return rc;
	
ov3640_set_ae_gain_fail :
	printk("#### ov3640_set_ae_gain fail####\n");
	return rc;
}
static long ov3640_set_contrast(int mode, int8_t contrast)
{
    long rc = 0;
#if 0
    unsigned short Temp = 0;
    printk("#### %s ####\n", __FUNCTION__);
    rc = ov3640_i2c_read(ov3640_client->addr, 0x3355, &Temp, WORD_LEN);
    if (rc < 0)
        goto ov3640_set_contrast_fail;
    
    switch (mode) {
        case SENSOR_PREVIEW_MODE:
        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
        case SENSOR_VIDEO_MODE:
            printk("#### ov3640_set_contrast %d ####\n", contrast);
            Temp |= 0x4;
            switch (contrast)
            {
            case CAMSENSOR_CONTRAST_LEVEL_0:
              rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0x4, BYTE_LEN); 
              rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_contrast_fail;
              break;
            case CAMSENSOR_CONTRAST_LEVEL_1:
                rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0x8, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_contrast_fail;
                break;
            case CAMSENSOR_CONTRAST_LEVEL_2:
                rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0xc, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_contrast_fail;
                break;
            case CAMSENSOR_CONTRAST_LEVEL_3:
              rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0x20, BYTE_LEN); 
              rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_contrast_fail;
              break;
            case CAMSENSOR_CONTRAST_LEVEL_4:
                rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0xf, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_contrast_fail;
                break;
            case CAMSENSOR_CONTRAST_LEVEL_5:
                rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0x14, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_contrast_fail;
                break;
            default:
              Temp &= (~0x4);
              rc = ov3640_i2c_write(ov3640_client->addr,0x335d,0x20, BYTE_LEN); 
              rc = ov3640_i2c_write(ov3640_client->addr, 0x3355, (unsigned short)Temp, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_contrast_fail;
              break;
          }
          break;
        }
        return rc;
      ov3640_set_contrast_fail:
        printk("#### ov3640_set_contrast_fail ####\n");
#endif
     return rc;
}

static long ov3640_set_saturation(int mode, int8_t saturation)
{
    long rc = 0;
#if 0
    printk("#### %s ####\n", __FUNCTION__);
       
    switch (mode) {
        case SENSOR_PREVIEW_MODE:
        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
        case SENSOR_VIDEO_MODE:
            printk("#### ov3640_set_saturation %d ####\n", saturation);
            switch (saturation)
            {
            case CAMSENSOR_SATURATION_LEVEL_0:
              rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0x40, BYTE_LEN); 
              rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0x40, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_saturation_fail;
              break;
            case CAMSENSOR_SATURATION_LEVEL_1:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0x90, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0x90, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_saturation_fail;
                break;
            case CAMSENSOR_SATURATION_LEVEL_2:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0xb0, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0xb0, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_saturation_fail;
                break;
            case CAMSENSOR_SATURATION_LEVEL_3:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0xd0, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0xd0, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_saturation_fail;
              break;
            case CAMSENSOR_SATURATION_LEVEL_4:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0xe0, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0xe0, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_saturation_fail;
                break;
            case CAMSENSOR_SATURATION_LEVEL_5:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0xf0, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0xf0, BYTE_LEN); 
                if (rc < 0)
                  goto ov3640_set_saturation_fail;
                break;
            default:
                rc = ov3640_i2c_write(ov3640_client->addr,0x3040,0xd0, BYTE_LEN); 
                rc = ov3640_i2c_write(ov3640_client->addr,0x3041,0xd0, BYTE_LEN); 
              if (rc < 0)
                goto ov3640_set_saturation_fail;
              break;
          }
          break;
        }
        return rc;
     ov3640_set_saturation_fail:
        printk("#### ov3640_set_saturation_fail ####\n");
#endif
    return rc;
}

static long ov3640_set_sharpness(int mode, int8_t sharpness)
    {
        long rc = 0;
#if 0
        printk("#### %s ####\n", __FUNCTION__);
           
        switch (mode) {
            case SENSOR_PREVIEW_MODE:
            case SENSOR_SNAPSHOT_MODE:
            case SENSOR_RAW_SNAPSHOT_MODE:
            case SENSOR_VIDEO_MODE:
                printk("#### ov3640_set_sharpness %d ####\n", sharpness);
                switch (sharpness)
                {
                case CAMSENSOR_SHARPNESS_LEVEL_0:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x1, BYTE_LEN); 
                  if (rc < 0)
                    goto ov3640_set_sharpness_fail;
                  break;
                case CAMSENSOR_SHARPNESS_LEVEL_1:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x2, BYTE_LEN); 
                    if (rc < 0)
                      goto ov3640_set_sharpness_fail;
                    break;
                case CAMSENSOR_SHARPNESS_LEVEL_2:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x3, BYTE_LEN); 
                    if (rc < 0)
                      goto ov3640_set_sharpness_fail;
                    break;
                case CAMSENSOR_SHARPNESS_LEVEL_3:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x5, BYTE_LEN); 
                  if (rc < 0)
                    goto ov3640_set_sharpness_fail;
                  break;
                case CAMSENSOR_SHARPNESS_LEVEL_4:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x6, BYTE_LEN); 
                    if (rc < 0)
                      goto ov3640_set_sharpness_fail;
                    break;
                case CAMSENSOR_SHARPNESS_LEVEL_5:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x7, BYTE_LEN); 
                    if (rc < 0)
                      goto ov3640_set_sharpness_fail;
                    break;
                default:
                    rc = ov3640_i2c_write(ov3640_client->addr,0x302e,0x5, BYTE_LEN); 
                  if (rc < 0)
                    goto ov3640_set_sharpness_fail;
                  break;
              }
              break;
            }
            return rc;
         ov3640_set_sharpness_fail:
            printk("#### ov3640_set_sharpness_fail ####\n");
#endif
        return rc;
    }

static int32_t ov3640_get_af_done(int8_t *done)
{
	int rc = 0;
	unsigned short Temp=0;	

	//printk("#### %s ####\n", __FUNCTION__);

	rc = ov3640_i2c_read(ov3640_client->addr, 0x3607,&Temp, WORD_LEN);
	if (rc < 0)
		goto ov3640_get_af_done_fail;

	if (Temp==0x80)
		*done = 1;
	else
		*done = 0;
	
//ov3640_get_af_done_done:
	//printk("#### AF Status Result = %d####\n", *done);
	return rc;
	
ov3640_get_af_done_fail :
	printk("#### ov3640_get_ae_status fail####\n");
	return rc;
}


static int ov3640_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	int32_t i=0;
	int32_t array_length=0;
	unsigned short bTemp=0;/*CCI, 20100809, For new device DW7914 AF setting */
	unsigned short Temp=0; //	internal LDO => external LDO  reg 0x30A9 [3] to 1
	struct vreg *vreg_cam_dcore;

	ov3640_get_sensor_version();

    if (ov3640_version == CAMERA_TYPE_R2D) // use internal VDD to turn off DCORE
    {
        printk("#### R2D sensor to turn off DCORE power ####\n");
        vreg_cam_dcore=vreg_get(0, "rftx");	//VDD_D
        rc = vreg_set_level(vreg_cam_dcore,0);
        if (rc)
            printk("#### vreg set rftx level failed ####\n");

        rc = vreg_disable(vreg_cam_dcore);
        if (rc)
            printk("#### vreg disable vreg_cam_dcore level failed ####\n");

        mdelay(10);
	}

	array_length = ov3640_regs.init_reg_settings_size;
	printk("OV3640 initial register setting start\n");

	//Do Software reset
	rc = ov3640_i2c_write(ov3640_client->addr, 0x3012, 0x80, BYTE_LEN);
	mdelay(10);

	if (rc < 0)
			goto init_probe_fail;

    //  internal LDO => external LDO  reg 0x30A9 [3] to 1
    rc = ov3640_i2c_read(ov3640_client->addr, 0x30A9,&Temp, WORD_LEN);
    if (rc < 0)
    {
        printk("internal LDO => external LDO  reg 0x30A9 [3]  read error");
        goto init_probe_fail;
    }

    (ov3640_version == CAMERA_TYPE_R2C)? (Temp |= 0x08): (Temp &= ~(0x08));
	printk("set 0x30A9 = 0x%x.\n", Temp);

    rc = ov3640_i2c_write(ov3640_client->addr, 0x30A9, Temp, BYTE_LEN);
    if (rc < 0)
    {
        printk("internal LDO => external LDO  reg 0x30A9 [3]  write error");
        goto init_probe_fail;
    }

	for (i = 0; i < array_length; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.init_reg_settings[i].register_address,
		  ov3640_regs.init_reg_settings[i].register_value,
		  BYTE_LEN);

		if (rc < 0)
			goto init_probe_fail;
		udelay(10);
	}
	printk("OV3640 initial register setting end\n");

	mdelay(10);

	printk("OV3640 AF register setting start\n");
	#if 0
	array_length = ov3640_regs.AF_reg_settings_size;	
	for (i = 0; i < array_length; i++) {
		rc = ov3640_i2c_write(ov3640_client->addr,
		  ov3640_regs.AF_reg_settings[i].register_address,
		  ov3640_regs.AF_reg_settings[i].register_value,
		  BYTE_LEN);

		if (rc < 0)
			goto init_probe_fail;
		udelay(10);
	}
	#else	
	/*B: CCI, 20100809, For new device DW7914 AF setting */
	// 1. read OTP first
	// 1.1 write 0x308F as 0x55
	// 1.2 read 0x30FC
	// 2. if data of 0x30FC is 0x97 --> set NEW_AF_settings = DW9714. 
	//     else if data of 0x30FC is 0x00 --> set NEW_AF_settings = Allegro
	//     Default setting is Allegro

	rc =ov3640_i2c_write(ov3640_client->addr,0x308f, 0x55, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	
	rc = ov3640_i2c_read(ov3640_client->addr, 0x30fc, &bTemp, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;
	printk("#### ov3640_OTP read 0x30FC as 0x%x ####\n", bTemp);
	/*E: CCI, 20100809, For new device DW7914 AF setting */
	
	rc =ov3640_i2c_write(ov3640_client->addr,0x308c, 0x00, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3104, 0x02, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3105, 0xff, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3106, 0x00, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3107, 0xff, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;

	/*B: CCI, 20100809, For new device DW7914 AF setting */
	if (bTemp == 0x97)
	{
		rc = ov3640_i2c_txdata(ov3640_client->addr, ov3640_regs.NEW_AF_settings_DW9714, ov3640_regs.NEW_AF_settings_size_DW9714);
		if (rc < 0)
			goto init_probe_fail;
		printk("#### ov3640 _OTP set NEW_AF_settings = DW9714 ####\n");
	}
	else  //default is Allegro
	{
		rc = ov3640_i2c_txdata(ov3640_client->addr, ov3640_regs.NEW_AF_settings, ov3640_regs.NEW_AF_settings_size);
		if (rc < 0)
			goto init_probe_fail;
		printk("#### ov3640 _OTP set NEW_AF_settings = Allegro ####\n");
	}
	/*E: CCI, 20100809, For new device DW7914 AF setting */
	
	rc =ov3640_i2c_write(ov3640_client->addr,0x3104, 0x00, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3f06, 0x02, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3f00, 0x14, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	#if 0
	rc =ov3640_i2c_write(ov3640_client->addr,0x3011, 0x00, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3014, 0x2c, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3f06, 0x02, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3f00, 0x14, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	rc =ov3640_i2c_write(ov3640_client->addr,0x3606, 0x14, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;
	#endif
	#endif
	printk("OV3640 AF register setting end\n");
	
	return rc;

init_probe_fail:
	printk("#### init_probe_fail ####\n");
	return rc;
}

int ov3640_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	printk("#### %s ####\n", __FUNCTION__);
        //CH, 20100721, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
	wake_lock_init(&ov3640_wake_lock, WAKE_LOCK_SUSPEND, "ov3640");
        wake_lock(&ov3640_wake_lock);
        //CH, end
	ov3640_ctrl = kzalloc(sizeof(struct ov3640_ctrl), GFP_KERNEL);
	if (!ov3640_ctrl) {
		printk("ov3640_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		ov3640_ctrl->sensordata = data;

	LENC = true;
//CH, 20101014, [BugZliia 818] Launch Camera AP phone device auto reset.
	mdelay(20);
	msm_camio_camif_pad_reg_reset();
	mdelay(10);

	rc = ov3640_power_on_sequence(data);
	if(rc<0)
	  goto init_fail;
//end
	rc = ov3640_sensor_init_probe(data);
	if (rc < 0) {
		printk("ov3640_sensor_init failed!\n");
		goto init_fail;
	}	

init_done:
	return rc;

init_fail:
        //CH, 20100721, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
        wake_unlock(&ov3640_wake_lock);
	wake_lock_destroy(&ov3640_wake_lock);
        //CH, end
	kfree(ov3640_ctrl);
	return rc;
}

static int ov3640_init_client(struct i2c_client *client)
{
	printk("#### %s ####\n", __FUNCTION__);
	/* Initialize the MSM_CAMI2C Chip */	
	init_waitqueue_head(&ov3640_wait_queue);
	return 0;
}

int ov3640_sensor_config(void __user *argp)
{
  struct sensor_cfg_data cfg_data;
  long rc = 0;
  //printk("#### %s ####\n", __FUNCTION__);
  if (copy_from_user(
    &cfg_data,
    (void *)argp,
    sizeof(struct sensor_cfg_data)))
    return -EFAULT;

  down(&ov3640_sem); 

  //printk("ov3640_ioctl, cfgtype = %d, mode = %d\n",
  //cfg_data.cfgtype, cfg_data.mode);

  switch (cfg_data.cfgtype) {
    case CFG_SET_MODE:
      rc = ov3640_set_sensor_mode(
        cfg_data.mode);
      break;

    case CFG_SET_EFFECT:
      rc = ov3640_set_effect(
        cfg_data.mode,
        cfg_data.cfg.effect);
      break;
#if 1
    case CFG_SET_WB:
      rc = ov3640_set_wb(
        cfg_data.mode,
        cfg_data.cfg.wb);
      break;

    case CFG_SET_BRIGHTNESS:
      rc = ov3640_set_brightness(
        cfg_data.mode,
        cfg_data.cfg.brightness);
      break;
    case CFG_SET_ANTIBANDING:
      rc = ov3640_set_antibanding(
        cfg_data.mode,
        cfg_data.cfg.antibanding);
      break;
    case CFG_SET_FOCUS_MODE:
      rc = ov3640_set_focus_mode(
        cfg_data.mode,
        cfg_data.cfg.focus_mode);
      break;
    case CFG_SET_ISO:
      rc = ov3640_set_iso(
        cfg_data.mode,
        cfg_data.cfg.iso);
      break;
    case CFG_SET_EXPOSURE_MODE:
      rc = ov3640_set_exposure_mode(
        cfg_data.mode,
        cfg_data.cfg.ae_mode);
      break;
    case CFG_SET_LENS_SHADING:
      rc = ov3640_set_lens_shading_correction(
        cfg_data.mode,
        cfg_data.cfg.lens_shading);
      break;
    case CFG_SET_NIGHT_MODE:
      rc = ov3640_set_night_mode(
        cfg_data.mode,
        cfg_data.cfg.night_mode);
      break;
    case CFG_MOVE_FOCUS:
      rc =
        ov3640_move_focus(
        cfg_data.cfg.focus.dir,
        cfg_data.cfg.focus.steps);
      break;
    case CFG_SET_ZOOM_AE:
      rc =
        ov3640_set_zoom_ae(
        cfg_data.mode,
        cfg_data.cfg.zoom);
      break;
    case CFG_GET_AE_STATUS:
    {
      uint8_t* sdata = &cfg_data.cfg.ae_fps;
      rc = ov3640_get_ae_status(sdata);
      if (!rc && copy_to_user(argp, &cfg_data, sizeof(struct sensor_cfg_data)))
      rc = -EFAULT;
    }
    break;
    case CFG_SET_AE_GAIN:
      //printk("#### CFG_SET_AE_GAIN ####\n");
      rc =ov3640_set_ae_gain(cfg_data.mode,cfg_data.cfg.ae_gain);
      break;
    case CFG_SET_CONTRAST:
      rc = ov3640_set_contrast(
        cfg_data.mode,
        cfg_data.cfg.contrast);
      break;
    case CFG_SET_SATURATION:
      rc = ov3640_set_saturation(
        cfg_data.mode,
        cfg_data.cfg.saturation);
      break;
    case CFG_SET_SHARPNESS:
      rc = ov3640_set_sharpness(
        cfg_data.mode,
        cfg_data.cfg.sharpness);
	break;		
	case CFG_GET_FOCUS_DONE:
		{
			uint8_t	*sdata = &cfg_data.cfg.focus_done;
			rc = ov3640_get_af_done(sdata);
			if (!rc && copy_to_user(argp, &cfg_data, sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
		}
      break;

#endif
    default:
      rc = -EFAULT;
      break;
  }

  up(&ov3640_sem); 

  return rc;
}

int ov3640_sensor_release(void)
{
	int rc = 0;
	 down(&ov3640_sem); 
	 
	printk("#### %s ####\n", __FUNCTION__);
	ov3640_power_off_sequence();

	LENC = true;
        //CH, 20100721, TK13108, [KB62] A phone often  ANR if we quickly press PWR key again and again.
        wake_unlock(&ov3640_wake_lock);
	wake_lock_destroy(&ov3640_wake_lock);
        //CH, end
	kfree(ov3640_ctrl);
	ov3640_ctrl=NULL;
	 up(&ov3640_sem); 

	return rc;
}

static int ov3640_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	printk("#### %s ####\n", __FUNCTION__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov3640_sensorw =
		kzalloc(sizeof(struct ov3640_work_t), GFP_KERNEL);

	if (!ov3640_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov3640_sensorw);
	ov3640_init_client(client);
	ov3640_client = client;
	ov3640_client->addr = ov3640_client->addr >> 1;
	mdelay(50);

	printk("ov3640_probe successed!\n");

	return 0;

probe_failure:
	kfree(ov3640_sensorw);
	ov3640_sensorw = NULL;
	printk("ov3640_probe failed!\n");
	return rc;
}
#if 0
static int __exit ov3640_remove(struct i2c_client *client)
{
	struct ov3640_work_t *sensorw = i2c_get_clientdata(client);
	printk("#### %s ####\n", __FUNCTION__);

	free_irq(client->irq, sensorw);
	ov3640_client = NULL;
	ov3640_sensorw = NULL;
	kfree(sensorw);
	return 0;
}
#endif
static const struct i2c_device_id ov3640_i2c_id[] = {
	{ "ov3640", 0},
	{ },
};

static struct i2c_driver ov3640_i2c_driver = {
	.id_table = ov3640_i2c_id,
	.probe  = ov3640_i2c_probe,
	.remove = __exit_p(ov3640_i2c_remove),
	.driver = {
		.name = "ov3640",
	},
};
#if 0
static int32_t ov3640_init(void)
{
	int32_t rc = 0;

	printk("#### %s ####\n", __FUNCTION__);
	rc = i2c_add_driver(&ov3640_driver);
	if (IS_ERR_VALUE(rc))
		goto init_failure;
	return rc;

init_failure:
	printk("ov3640_init, rc = %d\n", rc);
	return rc;
}

void ov3640_exit(void)
{
	printk("#### %s ####\n", __FUNCTION__);

	i2c_del_driver(&ov3640_driver);
}
#endif
int ov3640_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc;

	ov3640_version = CAMERA_TYPE_R2D;

	printk("#### %s ####\n", __FUNCTION__);
	rc = ov3640_power_on_sequence(info);
	if(rc<0)
		goto probe_done;
	
	mdelay(20);
	
	rc = i2c_add_driver(&ov3640_i2c_driver);
	if (rc < 0 || ov3640_client == NULL) {
		rc = -ENOTSUPP;
	  goto probe_done;
	}

	rc = ov3640_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;

	s->s_init = ov3640_sensor_init;
	s->s_release = ov3640_sensor_release;
	s->s_config  = ov3640_sensor_config;

probe_done:
	printk("%s %s:%d\n", __FILE__, __func__, __LINE__);
		ov3640_power_off_sequence();
	return rc;
}

static int __ov3640_probe(struct platform_device *pdev)
{
	printk("#### %s ####\n", __FUNCTION__);
	return msm_camera_drv_start(pdev, ov3640_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov3640_probe,
	.suspend = msm_camera_suspend,
	.resume = msm_camera_resume,
	.driver = {
		.name = "msm_camera_ov3640",
		.owner = THIS_MODULE,
	},
};

static int __init ov3640_init(void)
{
	printk("#### %s ####\n", __FUNCTION__);
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov3640_init);

