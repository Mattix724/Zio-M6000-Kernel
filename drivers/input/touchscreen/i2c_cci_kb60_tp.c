/* COMPALCOMM.INC, Author: albert_wu@compalcomm.com
 * i2c_cci_kb60_tp.c - Touch screen driver for CCI KB60
 * copy from
 * Copyright (C) 2009 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@xxxxxxxxxxx>
 *
 * Based on wm97xx-core.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <linux/jiffies.h>
#include <linux/delay.h>


#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/msm_rpcrouter.h>
#include <mach/msm_rpcrouter.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/vreg.h>
#include <linux/delay.h>
#include <linux/timer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "mcs6000.h"


/*---------------------  Static Definitions -------------------------*/
#define HENRY_DEBUG 0   //0:disable, 1:enable
#if(HENRY_DEBUG)
    #define Printhh(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define Printhh(string, args...)
#endif

#define HENRY_TIP 1 //give RD information. Set 1 if develop,and set 0 when release.
#if(HENRY_TIP)
    #define PrintTip(string, args...)    printk("Henry(K)=> "string, ##args);
#else
    #define PrintTip(string, args...)
#endif
/*---------------------  Static Classes  ----------------------------*/

#define TS_CE 84

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend ts_early;
#endif

#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
extern int DVT_1_3;
extern int HW_ID4;
#endif

int IS_DVT_1_3 = 0;
int TOUCH_GPIO = 34;

/* Registers */
#define MCS6000_TS_STATUS		0x00
#define STATUS_OFFSET			0
#define STATUS_NO			(0 << STATUS_OFFSET)
#define STATUS_INIT			(1 << STATUS_OFFSET)
#define STATUS_SENSING			(2 << STATUS_OFFSET)
#define STATUS_COORD			(3 << STATUS_OFFSET)
#define STATUS_GESTURE			(4 << STATUS_OFFSET)
#define ERROR_OFFSET			4
#define ERROR_NO			(0 << ERROR_OFFSET)
#define ERROR_POWER_ON_RESET		(1 << ERROR_OFFSET)
#define ERROR_INT_RESET			(2 << ERROR_OFFSET)
#define ERROR_EXT_RESET			(3 << ERROR_OFFSET)
#define ERROR_INVALID_REG_ADDRESS	(8 << ERROR_OFFSET)
#define ERROR_INVALID_REG_VALUE		(9 << ERROR_OFFSET)

#define MCS6000_TS_OP_MODE		0x01
#define RESET_OFFSET			0
#define RESET_NO			(0 << RESET_OFFSET)
#define RESET_EXT_SOFT			(1 << RESET_OFFSET)

// Operating Mode for new firmware
#define OP_MODE_SLEEP					0x10 	//0001 0000
#define OP_MODE_ACTIVE				0x12	//0001 0010 
#define OP_MODE_IDEL					0x14	//0001 0100 
#define OP_MODE_SIDE_QWERTY		0x16	//0001 0110 	
#define OP_MODE_FAST					0x18	//0001 1000 

#define GESTURE_OFFSET			4
#define GESTURE_DISABLE			(0 << GESTURE_OFFSET)
#define GESTURE_ENABLE			(1 << GESTURE_OFFSET)
#define PROXIMITY_OFFSET		5
#define PROXIMITY_DISABLE		(0 << PROXIMITY_OFFSET)
#define PROXIMITY_ENABLE		(1 << PROXIMITY_OFFSET)
#define SCAN_MODE_OFFSET		6
#define SCAN_MODE_INTERRUPT		(0 << SCAN_MODE_OFFSET)
#define SCAN_MODE_POLLING		(1 << SCAN_MODE_OFFSET)
#define REPORT_RATE_OFFSET		7
#define REPORT_RATE_40			(0 << REPORT_RATE_OFFSET)
#define REPORT_RATE_80			(1 << REPORT_RATE_OFFSET)


#define MCS6000_TS_X_SIZE_UPPER		0x08
#define MCS6000_TS_X_SIZE_LOWER		0x09
#define MCS6000_TS_Y_SIZE_UPPER		0x0A
#define MCS6000_TS_Y_SIZE_LOWER		0x0B

#define MCS6000_TS_INPUT_INFO		0x10
#define INPUT_TYPE_OFFSET		0
#define INPUT_TYPE_NONTOUCH		(0 << INPUT_TYPE_OFFSET)
#define INPUT_TYPE_SINGLE		(1 << INPUT_TYPE_OFFSET)
#define INPUT_TYPE_DUAL			(2 << INPUT_TYPE_OFFSET)
#define INPUT_TYPE_PALM			(3 << INPUT_TYPE_OFFSET)
#define BUTTON_STATE_OFFSET		6
#define BUTTON_STATE_NONTOUCH		(0 << BUTTON_STATE_OFFSET)
#define BUTTON_STATE_TOUCH			(1 << BUTTON_STATE_OFFSET)



#define MCS6000_TS_XY_POS_UPPER		0x11
#define MCS6000_TS_X_POS_LOWER		0x12
#define MCS6000_TS_Y_POS_LOWER		0x13

#define MCS6000_TS_BUTTON               0x16
#define BUTTON_1_NONTOUCH               (0 << 0)
#define BUTTON_1_TOUCHED                (1 << 0)
#define BUTTON_2_NONTOUCH               (0 << 1)
#define BUTTON_2_TOUCHED                (1 << 1)
#define BUTTON_3_NONTOUCH               (0 << 2)
#define BUTTON_3_TOUCHED                (1 << 2)
#define BUTTON_4_NONTOUCH               (0 << 3)
#define BUTTON_4_TOUCHED                (1 << 3)
#define MCS6000_FW_VERSION               0x20
#define MCS6000_SN_VERSION               0x23


/* Touchscreen absolute values */
#define MCS6000_MAX_XC			0x1DF//0x13F//0x3ff
#define MCS6000_MAX_YC			0x31F//x1DF//0x3ff

//#define TOUCH_GPIO  34

#if defined(CONFIG_TOUCHSCREEN_CCI_KB60) || defined(CONFIG_TOUCHSCREEN_CCI_MXT224)
extern struct input_dev *tskpdev;
#endif


//////////////////
//DIO compiler option debug
//#define TRACE_SEN_DATA			// add trace sen data function
#define SEND_RELEASE_EVENT		// enable to send release event
//#define RANDOM_MOVE_GOOGLE_MAP		// enable to auto test - random move google map
//#define DEBUG_TRACE_TP_DOWN_UP_EVENT 		// for debug use, print first down event and release event.

//Dio
static u8 tp_fw_version = 0;
static u8 tp_sensor_reversion = 0;
int kb_x_firstTouch=0;
int kb_y_firstTouch=0;
int moved = 0;
int isReleased =0;
int isAlreadGetData = 0; // use to tag if we already get TP data.
static struct proc_dir_entry *TP_proc_dir = NULL;
#define MODE_IME		1	//IME mode
#define MODE_NORMAL	0	//NORMAL mode
static int UI_MODE = MODE_NORMAL;
//for test build, use to check upper bound of multiple-touch
#define DEFAULT_VALUE -1

#define UI_OP_MODE_SLEEP					0
#define UI_OP_MODE_ACTIVE					1
//#define UI_OP_MODE_IDEL						2
#define UI_OP_MODE_SIDE_QWERTY		3	
#define UI_OP_MODE_FAST						4
int UI_OP_MODE=UI_OP_MODE_ACTIVE;
//the time to check if release event is sent
#define CHECK_RELEASE_EVENT_TIME_INT	50
#define RELEASE_EVENT_RECOVERY_TIMES	6//20
/////////////////////////////////////////////////////
// enable it only if you want to debug
//#define DEBUG_MEASURE_PERFORMANCE

/* platform data for the MELFAS MCS6000 touchscreen driver */
struct mcs6000_ts_platform_data {
	int x_size;
	int y_size;
};

enum mcs6000_ts_read_offset {
	READ_INPUT_INFO,
	READ_XY_POS_UPPER,
	READ_X_POS_LOWER,
	READ_Y_POS_LOWER,
	READ_BLOCK_SIZE,
};

/* Each client has this additional data */
struct mcs6000_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct ts_event_work;
	struct work_struct ts_OP_MODE_change;
#ifdef SEND_RELEASE_EVENT
	struct work_struct ts_release_event_work;
#endif
	struct mcs6000_ts_platform_data *platform_data;
	unsigned int irq;
	atomic_t irq_disable;
};

static unsigned long suspending;


struct timer_list ts_timer;
static struct workqueue_struct *TSSC_wq;

struct mcs6000_ts_data *ts_data;

static struct i2c_driver mcs6000_ts_driver;

int kb_x;
int kb_y;
int kb_z;
int kb_w;
int b1 = 0;
int b2 = 0;
int b3 = 0;
int b4 = 0;
int tp_down = 0;

void ts_timeout(unsigned long para);

#define KB_HOME1   102
#define KB_MENU   139
#define KB_BACK   158
#define KB_SEARCH 217

//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
extern unsigned long ts_active_time;
extern unsigned long key_active_time;
//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.

//DL MODE
#define MCSDL_RET_PREPARE_ERASE_FLASH_FAILED	0x0B
#define MCSDL_RET_ERASE_FLASH_FAILED			0x02
#define MCSDL_RET_READ_FLASH_FAILED				0x04
#define MCSDL_RET_ERASE_VERIFY_FAILED			0x03
#define MCSDL_RET_PREPARE_PROGRAM_FAILED		0x09
#define MCSDL_RET_PROGRAM_FLASH_FAILED			0x07
#define MCSDL_RET_PROGRAM_VERIFY_FAILED			0x0A

#define MCSDL_ISP_CMD_ERASE_TIMING				0x0F
#define MCSDL_ISP_ERASE_TIMING_VALUE_0			0x01
#define MCSDL_ISP_ERASE_TIMING_VALUE_1			0xD4
#define MCSDL_ISP_ERASE_TIMING_VALUE_2			0xC0
#define MCSDL_ISP_CMD_ERASE						0x02
#define MCSDL_ISP_CMD_READ_FLASH				0x04
#define MCSDL_ISP_CMD_PROGRAM_TIMING			0x0F
#define MCSDL_ISP_PROGRAM_TIMING_VALUE_0		0x00
#define MCSDL_ISP_PROGRAM_TIMING_VALUE_1		0x00
#define MCSDL_ISP_PROGRAM_TIMING_VALUE_2		0x78
#define MCSDL_ISP_CMD_PROGRAM_FLASH				0x03
#define MCSDL_ISP_CMD_RESET						0x07

#define MCSDL_ISP_ACK_PREPARE_ERASE_DONE		0x8F
#define MCSDL_RET_SUCCESS						0x00
#define MCSDL_ISP_ACK_ERASE_DONE				0x82
#define MELFAS_TRANSFER_LENGTH					64
#define MCSDL_MDS_ACK_READ_FLASH				0x84
#define MCSDL_I2C_ACK_PREPARE_PROGRAM			0x8F
#define MCSDL_MDS_ACK_PROGRAM_FLASH				0x83

/* platform data for the MELFAS MCS6000 touchscreen driver */
struct mcs6000_dl_platform_data {
	int x_size;
	int y_size;
};

/* Each client has this additional data */
struct mcs6000_dl_data {
	struct i2c_client *client;
//	struct input_dev *input_dev;
//	struct work_struct dl_event_work;
//	struct mcs6000_dl_platform_data *platform_data;
//	unsigned int irq;
//	atomic_t irq_disable;
};

struct mcs6000_dl_data *mcs6k_dl_data;

//struct mcs6000_dl_data *dl_data;
int CUR_TP_VER = 0x68;

//
int mcs6000_i2c_write(struct i2c_client *client, int reg, u8 data);

// MELFAS HEX Studio v0.4 [2008.08.18]

const uint16_t MELFAS_binary_nLength = 0xE900;	// 58.3 KBytes ( 59648 )

#define DL_I2C_CLK 60
#define DL_I2C_DAT 61


//Henry_lin, 20101115, [BugID 1013] MELFAS TP f/w 0x80 cause TP register read fail or enter download mode fail.
#ifdef CONFIG_TOUCHSCREEN_CCI_MXT224
extern int TPiIsExist(void);
#else
static int TPiIsExist(void){
    
    return false;
}
#endif
//Henry_lin
/*---------------------------------------------------------------*/


void msm_dl_i2c_mux(bool gpio, int *gpio_clk, int *gpio_dat)
{
	unsigned id;

	if (gpio) {
		id = GPIO_CFG(DL_I2C_CLK, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
		gpio_tlmm_config( id, 0);
		id = GPIO_CFG(DL_I2C_DAT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA);
        	gpio_tlmm_config( id, 0);
        	*gpio_clk = DL_I2C_CLK;
        	*gpio_dat = DL_I2C_DAT;
	} else {
		id = GPIO_CFG(DL_I2C_CLK, 1, GPIO_INPUT,GPIO_NO_PULL, GPIO_2MA);
		gpio_tlmm_config( id, 0);
		id = GPIO_CFG(DL_I2C_DAT , 1, GPIO_INPUT,GPIO_NO_PULL, GPIO_2MA);
		gpio_tlmm_config( id, 0);
	}
}

static int mcsdl_i2c_prepare_erase_flash(struct mcs6000_dl_data *data)
{
	struct i2c_client *client = data->client;
	int   nRet = MCSDL_RET_PREPARE_ERASE_FLASH_FAILED;
	int i;
	int   bRet;
	u8 i2c_buffer[4] = {	MCSDL_ISP_CMD_ERASE_TIMING,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_0,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_1,
	                        MCSDL_ISP_ERASE_TIMING_VALUE_2   };
	u8 ucTemp;

	//-----------------------------
	// Send Erase Setting code
	//-----------------------------

	for (i=0; i<4; i++) {
		bRet = i2c_master_send(client, &i2c_buffer[i], 1);//_i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );
		if (bRet != 1) {
			goto MCSDL_I2C_PREPARE_ERASE_FLASH_FINISH;
		}
		udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
	}

	//-----------------------------
	// Read Result
	//-----------------------------

	udelay(500);//mcsdl_delay(MCSDL_DELAY_500US);                  		// Delay 500usec

	bRet = i2c_master_recv(client, &ucTemp, 1);//_i2c_read_(MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if (bRet && ucTemp == MCSDL_ISP_ACK_PREPARE_ERASE_DONE) {
		nRet = MCSDL_RET_SUCCESS;
	}

MCSDL_I2C_PREPARE_ERASE_FLASH_FINISH :

	return nRet;

}

static int mcsdl_i2c_erase_flash(struct mcs6000_dl_data *data)
{
	int   nRet = MCSDL_RET_ERASE_FLASH_FAILED;
	struct i2c_client *client = data->client;
	u8 i;
	int bRet;
	u8 i2c_buffer[1] = {	MCSDL_ISP_CMD_ERASE};
	u8 ucTemp;

	//-----------------------------
	// Send Erase code
	//-----------------------------

	for (i=0; i<1; i++) {
		bRet = i2c_master_send(client, &i2c_buffer[i], 1);//_i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );
		if (bRet != 1)
			goto MCSDL_I2C_ERASE_FLASH_FINISH;
		udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
	}

	//-----------------------------
	// Read Result
	//-----------------------------

	mdelay(45);//mcsdl_delay(MCSDL_DELAY_45MS);                  			// Delay 45ms

	bRet = i2c_master_recv(client, &ucTemp, 1);//_i2c_read_(MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if (bRet && ucTemp == MCSDL_ISP_ACK_ERASE_DONE)
		nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_ERASE_FLASH_FINISH :

	return nRet;
}


static int mcsdl_i2c_read_flash( u8 *pBuffer, int nAddr_start, u8 cLength,struct mcs6000_dl_data *data)
{
	int nRet = MCSDL_RET_READ_FLASH_FAILED;
	struct i2c_client *client = data->client;
	int     i;
	int   bRet;
	u8   cmd[4];
	u8   ucTemp;

	//-----------------------------------------------------------------------------
	// Send Read Flash command   [ Read code - address high - address low - size ]
	//-----------------------------------------------------------------------------

	cmd[0] = MCSDL_ISP_CMD_READ_FLASH;
	cmd[1] = (u8)((nAddr_start >> 8 ) & 0xFF);
	cmd[2] = (u8)((nAddr_start      ) & 0xFF);
	cmd[3] = cLength;

	for (i=0; i<4; i++) {
		bRet = i2c_master_send(client, &cmd[i], 1);//_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &cmd[i], 1 );
		udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
		if (bRet != 1)
			goto MCSDL_I2C_READ_FLASH_FINISH;
	}

	//----------------------------------
	// Read 'Result of command'
	//----------------------------------
	bRet = i2c_master_recv(client, &ucTemp, 1 );//_i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &ucTemp, 1 );

	if ((bRet != 1) || ucTemp != MCSDL_MDS_ACK_READ_FLASH)
		goto MCSDL_I2C_READ_FLASH_FINISH;


	//----------------------------------
	// Read Data  [ pCmd[3] == Size ]
	//----------------------------------
	for (i=0; i<(int)cmd[3]; i++) {
		udelay(100);//mcsdl_delay(MCSDL_DELAY_100US);                  // Delay about 100us
		bRet = i2c_master_recv(client, pBuffer++, 1 );//_i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, pBuffer++, 1 );
		if (bRet != 1 && i!=(int)(cmd[3]-1))
			goto MCSDL_I2C_READ_FLASH_FINISH;
	}

	nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_READ_FLASH_FINISH :

	return nRet;
}

static int mcsdl_i2c_prepare_program(struct mcs6000_dl_data *data)
{
	int nRet = MCSDL_RET_PREPARE_PROGRAM_FAILED;
	struct i2c_client *client = data->client;
	int i;
	int bRet;
	u8 i2c_buffer[4] = { MCSDL_ISP_CMD_PROGRAM_TIMING,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_0,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_1,
		                    MCSDL_ISP_PROGRAM_TIMING_VALUE_2};
	u8 i2c_Ret;

	//------------------------------------------------------
	//   Write Program timing information
	//------------------------------------------------------
	for (i=0; i<4; i++) {
			bRet = i2c_master_send(client, &i2c_buffer[i], 1);//_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[i], 1 );
			if (bRet != 1)
				goto MCSDL_I2C_PREPARE_PROGRAM_FINISH;
			udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
	}
	udelay(500);//mcsdl_delay(MCSDL_DELAY_500US);                     // delay about  500us

	//------------------------------------------------------
	//   Read command's result
	//------------------------------------------------------
	bRet = i2c_master_recv(client, &i2c_Ret, 1 );//_i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &i2c_buffer[4], 1 );
	if ((bRet != 1) || i2c_Ret != MCSDL_I2C_ACK_PREPARE_PROGRAM)
		goto MCSDL_I2C_PREPARE_PROGRAM_FINISH;

	udelay(100);//mcsdl_delay(MCSDL_DELAY_100US);                     // delay about  100us

	nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_PREPARE_PROGRAM_FINISH :

	return nRet;

}


static int mcsdl_i2c_program_flash( u8 *pData, int nAddr_start, u8 cLength,struct mcs6000_dl_data *data)
{
	int nRet = MCSDL_RET_PROGRAM_FLASH_FAILED;
	struct i2c_client *client = data->client;
	int     i;
	int   bRet;
	u8    cData;
	u8    cmd[4];

	//-----------------------------
	// Send Read code
	//-----------------------------
	cmd[0] = MCSDL_ISP_CMD_PROGRAM_FLASH;
	cmd[1] = (u8)((nAddr_start >> 8 ) & 0xFF);
	cmd[2] = (u8)((nAddr_start      ) & 0xFF);
	cmd[3] = cLength;

	for (i=0; i<4; i++) {
		bRet = i2c_master_send(client, &cmd[i], 1);//_i2c_write_(MCSDL_I2C_SLAVE_ADDR_ORG, &cmd[i], 1 );
		udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
		if( bRet != 1 )
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;
	}
	//-----------------------------
	// Check command result
	//-----------------------------

	bRet = i2c_master_recv(client, &cData, 1);//_i2c_read_( MCSDL_I2C_SLAVE_ADDR_ORG, &cData, 1 );

	if ((bRet != 1) || cData != MCSDL_MDS_ACK_PROGRAM_FLASH)
		goto MCSDL_I2C_PROGRAM_FLASH_FINISH;

	//-----------------------------
	// Program Data
	//-----------------------------
	udelay(150);//mcsdl_delay(MCSDL_DELAY_150US);                  // Delay about 150us

	for (i=0; i<(int)cmd[3]; i+=2) {
		bRet = i2c_master_send(client, &pData[i+1], 1 );//_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &pData[i+1], 1 );
		if (bRet != 1)
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;
		udelay(100);//mcsdl_delay(MCSDL_DELAY_100US);                  // Delay about 150us

		bRet = i2c_master_send(client, &pData[i], 1  );//_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, &pData[i], 1 );
		udelay(150);//mcsdl_delay(MCSDL_DELAY_150US);                  // Delay about 150us

		if(bRet != 1)
			goto MCSDL_I2C_PROGRAM_FLASH_FINISH;
	}

	nRet = MCSDL_RET_SUCCESS;

MCSDL_I2C_PROGRAM_FLASH_FINISH :

	return nRet;
}

int mcs6000_dl_process(struct mcs6000_dl_data *dl_data)
{
	struct i2c_client *client = dl_data->client;
	u8  buffer[MELFAS_TRANSFER_LENGTH];
	int nStart_address = 0;
	u8 cLength;
	int rc, i;
	//struct vreg *vreg_ruim;
	int gpio_clk, gpio_dat;
	u8 *pOriginal_data;
	u8 enter_code[14] = { 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1 };
	u8 buf[1] = {0};

	printk("########################################\n");
	printk("%s...\n",__func__);
	printk("########################################\n");

	msm_dl_i2c_mux(true, &gpio_clk, &gpio_dat);
	gpio_direction_output(gpio_clk, 0);
	udelay(5);
	gpio_direction_output(gpio_dat, 0);
	udelay(5);

	gpio_configure(TOUCH_GPIO,GPIOF_DRIVE_OUTPUT);
	gpio_direction_output(TOUCH_GPIO,0);
	mdelay(90);

//	vreg_enable(vreg_ruim);
	gpio_configure(TS_CE,GPIOF_DRIVE_OUTPUT);
	gpio_direction_output(TS_CE,1);

	gpio_direction_output(TOUCH_GPIO,1);
	mdelay(20);
	gpio_direction_output(TOUCH_GPIO,0);
	mdelay(20);

	gpio_direction_output(gpio_dat, 1);
	mdelay(25);

	printk("Enter Download mode\n");
	for (i=0; i<14; i++) {
		if (enter_code[i])	{
			//TKEY_RESETB_SET_HIGH();
			gpio_direction_output(TOUCH_GPIO,1);//TKEY_INTR_SET_HIGH();
		} else {
			//TKEY_RESETB_SET_LOW();
			gpio_direction_output(TOUCH_GPIO,0);//TKEY_INTR_SET_LOW();
		}

		gpio_direction_output(gpio_clk, 1);//TKEY_I2C_SCL_SET_HIGH();
		udelay(15);//mcsdl_delay(MCSDL_DELAY_15US);
		gpio_direction_output(gpio_clk, 0);//TKEY_I2C_SCL_SET_LOW();

		//TKEY_RESETB_SET_LOW();
		gpio_direction_output(TOUCH_GPIO,0);//TKEY_INTR_SET_LOW();
		udelay(100);//mcsdl_delay(MCSDL_DELAY_100US);
   	}
	gpio_direction_output(gpio_clk, 1);//TKEY_I2C_SCL_SET_HIGH();
	udelay(100);//mcsdl_delay(MCSDL_DELAY_100US);
	gpio_direction_output(TOUCH_GPIO,1);//TKEY_INTR_SET_HIGH();
	//TKEY_RESETB_SET_HIGH();

	mdelay(2);
	msm_dl_i2c_mux(false, NULL, NULL);
	udelay(10);
	i2c_master_recv(client, buf, 1);

	if (buf[0] == 0x55) {
		printk("%s: buf[0] == 0x55\n",__FUNCTION__);
	} else {
		printk("%s: buf[0] unknown\n",__FUNCTION__);
		//Henry_lin, 20101115, [BugID 1013] MELFAS TP f/w 0x80 cause TP register read fail or enter download mode fail.
		buffer[0] = MCSDL_ISP_CMD_RESET;
		i2c_master_send(client, buffer, 1);
		mdelay(180);
		PrintTip("[%s] Send MCSDL_ISP_CMD_RESET\n", __FUNCTION__);
		//Henry_lin
		goto exit;
	}
	mdelay(1);
	printk("Enter Download mode...OK\n");

	printk("Erasing...\n");
	rc = mcsdl_i2c_prepare_erase_flash(dl_data);
	if (rc != MCSDL_RET_SUCCESS)
		goto exit;
	mdelay(1);
	printk("mcsdl_i2c_prepare_erase_flash...OK\n");
	rc = mcsdl_i2c_erase_flash(dl_data);
	if (rc != MCSDL_RET_SUCCESS)
		goto exit;
	mdelay(1);
	printk("Erasing...OK\n");

	printk("Verify Erasing...\n");
	rc = mcsdl_i2c_read_flash(buffer, 0x00, 16, dl_data);// Must be '0xFF' after erase
	if (rc != MCSDL_RET_SUCCESS)
		goto exit;
	for (i=0; i<16; i++) {
		if (buffer[i] != 0xFF) {
			rc = MCSDL_RET_ERASE_VERIFY_FAILED;
			goto exit;
		}
	}
	mdelay(1);
	printk("Verify Erasing...OK\n");

	printk("Preparing Program...\n");
	rc = mcsdl_i2c_prepare_program(dl_data);
	if (rc != MCSDL_RET_SUCCESS)
		goto exit;
	mdelay(1);
	printk("Preparing Program...OK\n");


	printk("Program flash...\n");
	pOriginal_data  = (u8 *)MELFAS_binary;

	nStart_address = 0;
	cLength  = MELFAS_TRANSFER_LENGTH;

	for (nStart_address = 0; nStart_address < MELFAS_binary_nLength; nStart_address+=cLength) {
		printk("#");

		if ((MELFAS_binary_nLength - nStart_address ) < MELFAS_TRANSFER_LENGTH) {
			cLength  = (u8)(MELFAS_binary_nLength - nStart_address);
			cLength += (cLength%2);									// For odd length.
		}

		rc = mcsdl_i2c_program_flash( pOriginal_data, nStart_address, cLength, dl_data );

		if (rc != MCSDL_RET_SUCCESS) {
			printk("\nProgram flash failed position : 0x%x / rc : 0x%x ", nStart_address, rc);
			goto exit;
		}
		pOriginal_data  += cLength;
		udelay(500);//mcsdl_delay(MCSDL_DELAY_500US);					// Delay '500 usec'
	}
	printk("\nProgram flash...OK\n");
	//--------------------------------------------------------------
	//
	// Verify flash
	//
	//--------------------------------------------------------------
	printk("Verify flash...\n");
	pOriginal_data  = (u8 *)MELFAS_binary;
	nStart_address = 0;

	cLength  = MELFAS_TRANSFER_LENGTH;

	for (nStart_address = 0; nStart_address < MELFAS_binary_nLength; nStart_address+=cLength) {
		printk("#");
		if ((MELFAS_binary_nLength - nStart_address ) < MELFAS_TRANSFER_LENGTH) {
			cLength = (u8)(MELFAS_binary_nLength - nStart_address);
			cLength += (cLength%2);									// For odd length.
		}

		//--------------------
		// Read flash
		//--------------------
		rc = mcsdl_i2c_read_flash(buffer, nStart_address, cLength,dl_data);

		//--------------------
		// Comparing
		//--------------------

		for (i=0; i<(int)cLength; i++) {
			if (buffer[i] != pOriginal_data[i]) {
				printk("\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", nStart_address, pOriginal_data[i], buffer[i] );
				rc = MCSDL_RET_PROGRAM_VERIFY_FAILED;
				goto exit;

			}
		}
		pOriginal_data += cLength;
		udelay(500);//mcsdl_delay(MCSDL_DELAY_500US);					// Delay '500 usec'
	}
	mdelay(1);//mcsdl_delay(MCSDL_DELAY_1MS);							// Delay '1 msec'
	printk("\nVerify flash...OK\n");

	//---------------------------
	//	Reset command
	//---------------------------
	buffer[0] = MCSDL_ISP_CMD_RESET;
	i2c_master_send(client, buffer, 1);//_i2c_write_( MCSDL_I2C_SLAVE_ADDR_ORG, buffer, 1 );

	//TKEY_INTR_SET_INPUT();									// Rollback Interrupt port

	//mcsdl_delay(MCSDL_DELAY_45MS);							// Delay about '200 msec'
	//mcsdl_delay(MCSDL_DELAY_45MS);
	//mcsdl_delay(MCSDL_DELAY_45MS);
	//mcsdl_delay(MCSDL_DELAY_45MS);
	mdelay(180);

	printk("%s Done\n",__FUNCTION__);
	return 0;

//exit_free:
//	kfree(dl_data);
//	i2c_set_clientdata(client, NULL);
exit:
	printk("%s Failed\n",__FUNCTION__);
	return (-1);
}


static int __devinit mcs6000_dl_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	struct mcs6000_dl_data *data;
	int ret;

	printk("########################################\n");
	printk("%s...\n",__func__);
	printk("########################################\n");
	data = kzalloc(sizeof(struct mcs6000_dl_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate driver data\n");
		ret = -ENOMEM;
		return ret;
	}
	data->client = client;
//	dl_data->platform_data = client->dev.platform_data;
//	dl_data->irq = MSM_GPIO_TO_INT(TOUCH_GPIO);//client->irq;
	mcs6k_dl_data = data;
//	atomic_set(&dl_data->irq_disable, 0);

	i2c_set_clientdata(client, data);

	return 0;
}


static const struct i2c_device_id mcs6000_dl_id[] = {
	{ "mcs6000_dl", 0 },
	{ }
};
static struct i2c_driver mcs6000_dl_driver = {
	.probe		= mcs6000_dl_probe,
	.id_table	= mcs6000_dl_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name = "mcs6000_dl",
	},
};
//


#ifdef DEBUG_MEASURE_PERFORMANCE

int getUiMode(void)
{
	return (int) UI_MODE;
}
EXPORT_SYMBOL(getUiMode);

unsigned int timeData[5];
#define TP_INTERRUPT				0
#define TP_READ_BEGIN			1
#define TP_READ_END				4
#define DISPLAY_READ_BEGIN		2
#define DISPLAY_READ_END		3

int firstIME = 0;

void debugTrace(int display)
{
	struct timeval tv;
	if (UI_MODE == MODE_IME) {
		switch (display) {
			case TP_INTERRUPT: // 
				do_gettimeofday(&tv);
				timeData[display] = tv.tv_sec * 1000 + tv.tv_usec / 1000;;			
				printk("DIOWORLD mcs6000_ts_interrupt() gettime %u\n", timeData[display]);
				firstIME = 1;
			break;			
			case TP_READ_BEGIN: // 
				do_gettimeofday(&tv);
				timeData[display] = tv.tv_sec * 1000 + tv.tv_usec / 1000;;						
				printk("DIOWORLD mcs6000_ts_input_read() Before %u\n", timeData[display]);
				printk("DIOWORLD mcs6000_ts_input_read() time inteval %u\n", timeData[1]-timeData[0]);		
			break;
			case DISPLAY_READ_BEGIN: // 
				if (firstIME == 1) {
					do_gettimeofday(&tv);
					timeData[display] = tv.tv_sec * 1000 + tv.tv_usec / 1000;;						
					printk("DIOWORLD msm_fb_pan_display() Before %u\n", timeData[display]);
				}
			break;
			case DISPLAY_READ_END: // 
				if (firstIME == 1) {
					do_gettimeofday(&tv);
					timeData[display] = tv.tv_sec * 1000 + tv.tv_usec / 1000;;						
					printk("DIOWORLD msm_fb_pan_display() After %u\n", timeData[display]);
					printk("DIOWORLD msm_fb_pan_display() time inteval %u\n", timeData[3]-timeData[2]);					
				}
			break;	
			case TP_READ_END: // 
				do_gettimeofday(&tv);
				timeData[display] = tv.tv_sec * 1000 + tv.tv_usec / 1000;;						
				printk("DIOWORLD mcs6000_ts_input_read() After %u\n", timeData[display]);
				printk("DIOWORLD mcs6000_ts_input_read() time inteval %u\n", timeData[4]-timeData[1]);					
			break;		
			
		}	
	}
}
EXPORT_SYMBOL(debugTrace);
#endif
//////////////////////////////////////////////////////

static int
proc_TP_data_read(char *page, char**start, off_t offset,
		                  int count, int *eof, void *data)
{
	int len = 0;
	len += sprintf(page + len, "%x", tp_fw_version);
	len += sprintf(page + len, "%x", tp_sensor_reversion);
	printk(KERN_ERR "[%s]:%s():count=%d,page=%s, len=%d \n", __FILE__, __func__, (int)count,(char *)page, len);
	return len;
}

static void mcs6000_ts_opmode_change_worker(struct work_struct *work)
{
	switch (UI_OP_MODE) {
		case UI_OP_MODE_ACTIVE:  
			printk("OP_MODE_ACTIVE\n");
			mcs6000_i2c_write(ts_data->client, MCS6000_TS_OP_MODE, OP_MODE_ACTIVE);
		break;
	        case UI_OP_MODE_SIDE_QWERTY:  
			printk("OP_MODE_SIDE_QWERTY\n");
			mcs6000_i2c_write(ts_data->client, MCS6000_TS_OP_MODE, OP_MODE_SIDE_QWERTY);
		break;
	        case UI_OP_MODE_FAST:          
			printk("OP_MODE_FAST\n");
			mcs6000_i2c_write(ts_data->client, MCS6000_TS_OP_MODE, OP_MODE_FAST);
		break;
	}
}

static int
proc_TP_data_write(struct file *file,
			const char __user * buffer,
			unsigned long count, void *data)
{
	if (test_bit(1, &suspending)) {
		printk("OP_MODE change in suspending\n");
		return 0;
	}
	if ('0'==*(buffer) && UI_OP_MODE != UI_OP_MODE_FAST) {
		UI_MODE=MODE_NORMAL;
		// set TP OP_MODE t0 UI_OP_MODE_FAST
		UI_OP_MODE= UI_OP_MODE_FAST;
	        queue_work(TSSC_wq, &ts_data->ts_OP_MODE_change);
	} else if ('1'==*(buffer) && UI_OP_MODE != UI_OP_MODE_SIDE_QWERTY) {
		UI_MODE=MODE_IME;
		// set TP OP_MODE t0 UI_OP_MODE_SIDE_QWERTY
		UI_OP_MODE= UI_OP_MODE_SIDE_QWERTY;
	        queue_work(TSSC_wq, &ts_data->ts_OP_MODE_change);
	} else if ('2'==*(buffer) && UI_OP_MODE != UI_OP_MODE_FAST) { // for set OP MODE
		UI_OP_MODE= UI_OP_MODE_FAST;
		queue_work(TSSC_wq, &ts_data->ts_OP_MODE_change);
/*
	else if('2'==*(buffer)) { // for set OP MODE
		++buffer;

        if(test_bit(1, &suspending))
        {
			printk("OP_MODE change in suspending\n");
             return 0;
        }
		if('1'==*(buffer) && UI_OP_MODE != UI_OP_MODE_ACTIVE) //OP_MODE_ACTIVE
		{
			UI_OP_MODE= UI_OP_MODE_ACTIVE;
//			printk("OP_MODE_ACTIVE\n");
//			mcs6000_i2c_write(ts_data->client, MCS6000_TS_OP_MODE, OP_MODE_ACTIVE);

            queue_work(TSSC_wq, &ts_data->ts_OP_MODE_change);
		}
		else if('4'==*(buffer) && UI_OP_MODE != UI_OP_MODE_FAST) //OP_MODE_FAST
		{
			UI_OP_MODE= UI_OP_MODE_FAST;
//			printk("OP_MODE_FAST\n");
//			mcs6000_i2c_write(ts_data->client, MCS6000_TS_OP_MODE, OP_MODE_FAST);

            queue_work(TSSC_wq, &ts_data->ts_OP_MODE_change);
		}        
*/
	}

//    printk("OP_MODE change UI_MODE = %d, UI_OPMODE= %d\n", UI_MODE, UI_OP_MODE);
	return 1;
}

static int init_tp_data_proc(void)
{

	//struct proc_dir_entry *lightsensor_proc_enable = NULL;
	struct proc_dir_entry *TP_proc_data = NULL;
	int retval = 0;

	/* Create /proc/USB */
	if (!(TP_proc_dir = proc_mkdir("TP", NULL))) {
		printk(KERN_ERR "[%s]:%s():%d --> Cannot create /proc/TP\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	} /*else {
		TP_proc_dir->owner = THIS_MODULE;
	}*/

	/* create data */
	if (!(TP_proc_data = create_proc_entry("data", 0666, TP_proc_dir))) {
		printk(KERN_ERR "[%s]:%s():%d --> Cannot create /proc/TP/data\n", __FILE__, __func__, __LINE__);
		retval = -1;
		goto end;
	} else {
		//TP_proc_data->owner = THIS_MODULE;
		TP_proc_data->read_proc = proc_TP_data_read;
		TP_proc_data->write_proc = proc_TP_data_write;
	}

end:
	return retval;
}

//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
extern void cci_ts_input_disable_delay(unsigned delay);
extern void cci_key_input_disable_delay(unsigned delay);
//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.

#ifdef CONFIG_HAS_EARLYSUSPEND
void ts_early_suspend(struct early_suspend *h);
void ts_later_resume(struct early_suspend *h);
#endif
#ifdef CONFIG_SLATE_TEST
extern struct input_dev *ts_input_dev;
#endif
static void reset_TP_state(void)
{
	struct vreg *vreg_ruim; 
	printk("%s: reset_TP_state\n", __func__);

	isReleased =0;
	isAlreadGetData = 0; // use to tag if we already get TP data.
	tp_down = 0;
	UI_OP_MODE=UI_OP_MODE_ACTIVE;

	gpio_direction_output(TOUCH_GPIO,0);

     
	vreg_ruim = vreg_get(NULL, "ruim");
	if (!IS_DVT_1_3) {
		vreg_ruim = vreg_get(NULL, "ruim");
		vreg_disable(vreg_ruim);
	} else if (IS_DVT_1_3) {
		gpio_direction_output(TS_CE,0);
	}
	mdelay(50);


	gpio_direction_output(TOUCH_GPIO,1);
	mdelay(20);
	if (!IS_DVT_1_3) {
		vreg_enable(vreg_ruim);
	} else if (IS_DVT_1_3) {
		gpio_direction_output(TS_CE,1);
	}
	mdelay(50);
	gpio_direction_input(TOUCH_GPIO);
}


#define retry_I2C 3
int mcs6000_i2c_write(struct i2c_client *client, int reg, u8 data)
{
	u8 buf[2];
	int rc;
	int ret = 0;

	int i = 0;


	if (test_bit(1, &suspending)) {
		printk("TP in suspending, i2c command write skip.\n");
        	return -1;
	}
	for (i = 0; i< retry_I2C ; i++) {
		buf[0] = reg;
		buf[1] = data;

		rc = i2c_master_send(client, buf, 2);
		if (rc != 2) {
			dev_err(&client->dev,"%s FAILED: writing to reg %d\n",__FUNCTION__, reg);
    			ret = -1;

			atomic_dec(&ts_data->irq_disable);
			disable_irq_nosync(ts_data->irq);
			reset_TP_state();
			atomic_inc(&ts_data->irq_disable);
			enable_irq(ts_data->irq);
		} else 
			break;
	}
	return ret;
}

int mcs6000_i2c_read(struct i2c_client *client, int reg, u8 *buf, int count)
{
	int rc = 0;
	int ret = 0;

	if (test_bit(1, &suspending)) {
		printk("TP in suspending, i2c command read skip.\n");
		return -1;
	}
	buf[0] = reg;
	rc = i2c_master_send(client, buf, 1);
	if (rc != 1) {
		dev_err(&client->dev,
			"%s FAILED: read of register %d\n",__FUNCTION__, reg);
		ret = -1;
		goto failed;
	}
	//memset(buf,0x00,count);
	rc = i2c_master_recv(client, buf, count);
	if (rc != count) {
		dev_err(&client->dev,
			"%s FAILED: read %d bytes from reg %d\n", __FUNCTION__,
			count, reg);
		ret = -1;
		goto failed;
	}

	return ret;
failed:
	reset_TP_state();
	return ret;
}

#if 0 //james@cci

int mcs6000_i2c_burst_rxdata(struct i2c_client *client, u8 *buf)
{
   unsigned short saddr = client->addr;
   unsigned char *rxdata = buf;
   
   //MCS6000_TS_XY_POS_UPPER 
        struct i2c_msg msgs[] = {
        {
                .addr   = saddr,
                .flags = 0,
                .len   = 1,
                .buf   = rxdata,
        },
        {
                .addr   = saddr,
                .flags = I2C_M_RD,
                .len   = 3,
                .buf   = (rxdata),
        }
        };

        unsigned char i=0;

        //printk("#### %s ####\n", __FUNCTION__);

        for (i=0; i<3; i++)
        {
                if (i2c_transfer(client->adapter, msgs, 2) >= 0) 
                {
                    //printk("rxdata value -> 0x%x, 0x%x, 0x%x\n",*(rxdata),*(rxdata+1),*(rxdata+2));
                    return 0;
                }else{
                    //mdelay(10);
                    printk("%s i2c_transfer error , retry i = %d\n",__FUNCTION__,i);
                }
        }
        //printk("mcs6000_i2c_burst_rxdata faild\n");
        return -EIO;
}
#endif 


#ifdef DEBUG_MEASURE_PERFORMANCE
struct timeval tv_dio;
unsigned int timedata_dio=0;
#endif

void handleReleaseEvent(void)
{
	//reset data
	isReleased = 1;
        isAlreadGetData = 0;

	if (tp_down) {
		//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
		if (time_after(ts_active_time, jiffies)) {
			goto Button;
		} else {
			//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
			//input_report_abs(data->input_dev, ABS_X, kb_x);
			//input_report_abs(data->input_dev, ABS_Y, kb_y);
			input_report_abs(ts_data->input_dev, ABS_PRESSURE, 0);
			input_report_key(ts_data->input_dev, BTN_TOUCH, !!0);
			input_sync(ts_data->input_dev);

			// reset data
			kb_x_firstTouch = DEFAULT_VALUE;
			kb_y_firstTouch = DEFAULT_VALUE;

			//printk("INPUT_TYPE_NONTOUCH: BTN_TOUCH\n");
			//B 2010/07/16 Jiahan_Li, [TK12955][BugID 531]The progress bar disappears momentarily when tapping the screen during playind video with YouTube app.
			tp_down = 0;
		}
	}//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
        //}else{
        //E 2010/07/16 Jiahan_Li, [TK12955][BugID 531]The progress bar disappears momentarily when tapping the screen during playind video with YouTube app.

//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.//            printk("INPUT_TYPE_NONTOUCH: KEYPAD ->");
Button:
	if (time_after(key_active_time, jiffies)) {
		return;
	} else {
//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
		if (tskpdev == NULL) {
			printk("tskpdev == NULL\n");
			return;
		}

		if (b1) {
			input_report_key(tskpdev, KB_HOME1, 0);
			input_sync(tskpdev);
			//printk(" KB_HOME\n");
		}
		if (b2) {
			input_report_key(tskpdev, KB_MENU, 0);
			input_sync(tskpdev);
			//printk(" KB_MENU\n");
		}
		if (b3) {
			input_report_key(tskpdev, KB_BACK, 0);
			input_sync(tskpdev);
			//printk(" KB_BACK\n");
		}
		if (b4) {
			input_report_key(tskpdev, KB_SEARCH, 0);
			input_sync(tskpdev);
			//printk(" KB_SEARCH\n");
	    	}
        //B 2010/07/16 Jiahan_Li, [TK12955][BugID 531]The progress bar disappears momentarily when tapping the screen during playind video with YouTube app.
	//}
		b1 = b2 = b3 = b4 = 0;
		//tp_down = 0;
        //E 2010/07/16 Jiahan_Li, [TK12955][BugID 531]The progress bar disappears momentarily when tapping the screen during playind video with YouTube app.
	}//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
}

// handle Press Event
void handlePressEvent(struct mcs6000_ts_data *data)
{
//	struct i2c_client *client = data->client;
//	char buffer[READ_BLOCK_SIZE];
//	int err;
	int dx_kb, dy_kb;

#ifdef DEBUG_MEASURE_PERFORMANCE
	unsigned int timedata_temp_dio = timedata_dio;
#endif


	//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.	 
	if (time_after(ts_active_time, jiffies)) 
		return;
	//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.

#ifdef TRACE_SEN_DATA
	printk(" x=%d, y=%d", kb_x, kb_y);
#endif
#ifdef DEBUG_MEASURE_PERFORMANCE
	do_gettimeofday(&tv_dio);
	timedata_dio = tv_dio.tv_sec * 1000 + tv_dio.tv_usec / 1000;;			
	printk("DIOWORLD ts_read() gettime %u, diff= %d\n", timedata_dio, timedata_dio-timedata_temp_dio);
	printk(" x= %d, y= %d\n", kb_x, kb_y);
#endif

#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
	if (isReleased ==1) {
		struct timeval tv;
		unsigned int timestamp = 0;
		do_gettimeofday(&tv);
		timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		printk("DIO first Press event kb_x=%d, kb_y=%d, %u\n", kb_x, kb_y, timestamp);
	}
#endif
	isReleased = 0;// Dio
	if (UI_MODE == MODE_IME) {
		//printk("INPUT_TYPE_SINGLE: MODE_IME\n");
		if (kb_x>=10) {
			kb_x -= 10;
			kb_x = (kb_x*480)/470; //int(kb_x*1.0213)
		}

		if (kb_x_firstTouch == DEFAULT_VALUE) {
			kb_x_firstTouch = kb_x;
			kb_y_firstTouch = kb_y;
		} else {
			dx_kb = kb_x_firstTouch-kb_x;
			dy_kb = kb_y_firstTouch-kb_y;

			if ((dx_kb>15) || (dx_kb<-15) ||(dy_kb>15) || (dy_kb<-15)) {
				moved = 1;
				kb_x_firstTouch = kb_x;
				kb_y_firstTouch = kb_y;
			} else {
				moved = 0;
				kb_x = kb_x_firstTouch ;
				kb_y = kb_y_firstTouch ;
			}
		}
#ifdef DEBUG_MEASURE_PERFORMANCE
//		debugTrace(TP_READ_BEGIN);
#endif
		// normal node
		input_report_abs(data->input_dev, ABS_X, kb_x);
		input_report_abs(data->input_dev, ABS_Y, kb_y);
		input_report_abs(data->input_dev, ABS_PRESSURE, 1);
		input_report_key(data->input_dev, BTN_TOUCH, !!1);
		input_sync(data->input_dev);
		
#ifdef DEBUG_MEASURE_PERFORMANCE
//		debugTrace(TP_READ_END);
#endif
	} else {// UI_MODE == MODE_NORMAL
		input_report_abs(data->input_dev, ABS_X, kb_x);
		input_report_abs(data->input_dev, ABS_Y, kb_y);
		input_report_abs(data->input_dev, ABS_PRESSURE, 1);
		input_report_key(data->input_dev, BTN_TOUCH, !!1);
		input_sync(data->input_dev);
	}
	tp_down = 1;

	// use to recovery Release Event if don't get event after 50ms
       if (isAlreadGetData < RELEASE_EVENT_RECOVERY_TIMES)
               mod_timer(&ts_timer, jiffies + msecs_to_jiffies(CHECK_RELEASE_EVENT_TIME_INT));
}

static void mcs6000_ts_input_read(struct mcs6000_ts_data *data)
{
	struct i2c_client *client = data->client;
	char buffer[READ_BLOCK_SIZE];
	int err;
	int intype;
	char button_state;

////////////////////////////////////////////////////////////////
//use to check op mode
/*
	if(UI_OP_MODE == UI_OP_MODE_FAST){
		err = mcs6000_i2c_read(client,MCS6000_TS_OP_MODE,&buffer[READ_INPUT_INFO],1);
		if (err < 0) {
			dev_err(&client->dev, "%s, err[%d] DIOWORLD UI_OP_MODE_FAST Only\n", __func__, err);
		}
		intype = buffer[READ_INPUT_INFO];
		printk("DIOWORLD UI_OP_MODE_FAST = %d, %x\n", intype, intype);
	}else{
		err = mcs6000_i2c_read(client,MCS6000_TS_OP_MODE,&buffer[READ_INPUT_INFO],1);
		if (err < 0) {
			dev_err(&client->dev, "%s, err[%d] DIOWORLD UI_OP_MODE_NORMAL Only\n", __func__, err);
		}
		intype = buffer[READ_INPUT_INFO];
		printk("DIOWORLD UI_OP_MODE_NORMAL = %d, %x\n", intype, intype);
	}
*/
////////////////////////////////////////////////////////////////
	
	err = mcs6000_i2c_read(client,MCS6000_TS_INPUT_INFO,&buffer[READ_INPUT_INFO],4);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		return;
	}

	intype = buffer[READ_INPUT_INFO];

	if (ts_active_time - jiffies > 2000)
		ts_active_time = INITIAL_JIFFIES;//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
	if (key_active_time - jiffies > 2000)
		key_active_time = INITIAL_JIFFIES;//2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.


	switch (intype) {
	case INPUT_TYPE_NONTOUCH:
		{
#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
			struct timeval tv;
			unsigned int timestamp = 0;
#endif
			handleReleaseEvent();

#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
			do_gettimeofday(&tv);
			timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			printk("DIO release by NORMAL!! %u\n", timestamp);
#endif
		}
		break;
	case INPUT_TYPE_SINGLE:
		kb_x = ((buffer[READ_XY_POS_UPPER] & 0x03) << 8) | buffer[READ_X_POS_LOWER];
		kb_y = ((buffer[READ_XY_POS_UPPER] & 0x0C) << 6) | buffer[READ_Y_POS_LOWER];
		handlePressEvent(data);
		break;
	case BUTTON_STATE_TOUCH:
	//B 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
	       if (time_after(key_active_time, jiffies)) {
			break;
	       } else {
	//E 2010/07/20 Jiahan_Li [TK11887][BugID 544]Specifications for preventing malfunctions.
			if (b1||b2||b3||b4) {
				if (tp_down) {
	                		//input_report_abs(data->input_dev, ABS_X, kb_x);
					//input_report_abs(data->input_dev, ABS_Y, kb_y);
					input_report_abs(data->input_dev, ABS_PRESSURE, 0);
				        input_report_key(data->input_dev, BTN_TOUCH, !!0);
				        input_sync(data->input_dev);
				        tp_down = 0;
				}
				//printk("BUTTON_STATE_TOUCH: REPEAT KEY break\n");
				break;
			}
			err = mcs6000_i2c_read(client,MCS6000_TS_BUTTON,&button_state,1);
			if (err < 0) {
				dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
				return;
			}

			if (tskpdev == NULL) {
				printk("tskpdev == NULL\n");
				return;
			}

			if (button_state & BUTTON_1_TOUCHED){
				if (!b1) {
					input_report_key(tskpdev, KB_HOME1, 1);
					input_sync(tskpdev);
					//printk("BUTTON_STATE_TOUCH: KB_HOME\n");
					b1 = 1;
				}
			}
			else if( button_state & BUTTON_2_TOUCHED ) {
				if (!b2) {
					input_report_key(tskpdev, KB_MENU, 1);
					input_sync(tskpdev);
					//printk("BUTTON_STATE_TOUCH: KB_MENU\n");
					b2 = 1;
				}
			}
			else if( button_state & BUTTON_3_TOUCHED ) {
				if (!b3) {
					input_report_key(tskpdev, KB_BACK, 1);
					input_sync(tskpdev);
					//printk("BUTTON_STATE_TOUCH: KB_BACK\n");
					b3 = 1;
				}
			}
			else if( button_state & BUTTON_4_TOUCHED ) {
				if (!b4) {
					input_report_key(tskpdev, KB_SEARCH, 1);
					input_sync(tskpdev);
					//printk("BUTTON_STATE_TOUCH: KB_SEARCH\n");
					b4 = 1;
				}
			}
		}
		break;	
	case INPUT_TYPE_DUAL:
#if 0
		{
			struct vreg *vreg_ruim;

			vreg_ruim = vreg_get(NULL, "ruim");
			dev_err(&client->dev, "Dual input type %x\n",
					buffer[READ_INPUT_INFO]);
			if(IS_DVT_1_3) {
				gpio_direction_output(TS_CE,0);
				msleep(20);
				gpio_direction_output(TS_CE,1);
				msleep(200);
				vreg_disable(vreg_ruim);
				msleep(20);
				vreg_enable(vreg_ruim);
				msleep(200);
			} else {

				vreg_disable(vreg_ruim);
				msleep(20);
				vreg_enable(vreg_ruim);
				msleep(200);
			}
		}
#endif
		break;
	case INPUT_TYPE_PALM:
#if 0
		{
			struct vreg *vreg_ruim;

			vreg_ruim = vreg_get(NULL, "ruim");
			dev_err(&client->dev, "Palm input type %x\n",
					buffer[READ_INPUT_INFO]);
			if(IS_DVT_1_3) {
				gpio_direction_output(TS_CE,0);
				msleep(20);
				gpio_direction_output(TS_CE,1);
				msleep(200);
				vreg_disable(vreg_ruim);
				msleep(20);
				vreg_enable(vreg_ruim);
				msleep(200);
			} else {

				vreg_disable(vreg_ruim);
				msleep(20);
				vreg_enable(vreg_ruim);
				msleep(200);
			}
		}
#endif
		break;
	default:
		printk("mcs6000_ts_input_read() status=%d is not except!!\n", intype);
		break;
	}
}

static void mcs6000_ts_irq_worker(struct work_struct *work)
{
	mcs6000_ts_input_read(ts_data);
	enable_irq(ts_data->irq);
	atomic_inc(&ts_data->irq_disable);
}

#ifdef SEND_RELEASE_EVENT
int counter_up =0;
int counter_down=0;
int counter_others=0;
static void mcs6000_ts_release_worker(struct work_struct *work)
{
	char buffer[READ_BLOCK_SIZE];
	int err;
	int intype;
	
	err = mcs6000_i2c_read(ts_data->client,MCS6000_TS_INPUT_INFO,&buffer[READ_INPUT_INFO],4);
	if (err < 0) {
		dev_err(&ts_data->client->dev, "%s, err[%d]\n", __func__, err);
		return;
	}

	intype = buffer[READ_INPUT_INFO];

	if (isAlreadGetData == (RELEASE_EVENT_RECOVERY_TIMES - 1)) {
#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
		struct timeval tv;
		unsigned int timestamp = 0;
#endif
		handleReleaseEvent();

#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
		do_gettimeofday(&tv);
		timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		printk("DIO FORCE Release!!! status=%d, %u\n", intype, timestamp);
#endif
		return;
	}
	switch (intype) {
		case INPUT_TYPE_NONTOUCH:
		{
#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
			struct timeval tv;
			unsigned int timestamp = 0;
#endif
			counter_up++;
			handleReleaseEvent();

#ifdef DEBUG_TRACE_TP_DOWN_UP_EVENT
			do_gettimeofday(&tv);
			timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
			printk("DIO release by RECOVERY!! %u\n", timestamp);
#endif
		}
			break;
		case INPUT_TYPE_SINGLE:
			counter_down++;
			kb_x = ((buffer[READ_XY_POS_UPPER] & 0x03) << 8) | buffer[READ_X_POS_LOWER];
			kb_y = ((buffer[READ_XY_POS_UPPER] & 0x0C) << 6) | buffer[READ_Y_POS_LOWER];
			handlePressEvent(ts_data);
			break;
		default:
			counter_others++;
			if (isAlreadGetData >0)
			    isAlreadGetData--;
			//handleReleaseEvent();
			break;
	}
	printk("DIO release_worker() status=%d, cup=%d, cdown=%d, cother=%d\n", intype, counter_up, counter_down, counter_others);

}
#endif

static irqreturn_t mcs6000_ts_interrupt(int irq, void *dev_id)
{
	struct mcs6000_ts_data *data = ts_data;

	if (!work_pending(&data->ts_event_work)) {
#ifdef DEBUG_MEASURE_PERFORMANCE
		//debugTrace(TP_INTERRUPT);
#endif   
		atomic_dec(&data->irq_disable);
		disable_irq_nosync(data->irq);
		//schedule_work(&data->ts_event_work);
		queue_work(TSSC_wq, &data->ts_event_work);
	}

	return IRQ_HANDLED;
}

void ts_timeout(unsigned long para)
{
	if (isReleased == 0) {
#ifdef SEND_RELEASE_EVENT
		struct timeval tv;
		unsigned int timestamp = 0;

               isAlreadGetData++;
               queue_work(TSSC_wq, &ts_data->ts_release_event_work);

		do_gettimeofday(&tv);
		timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		printk("Dio not Released rec times= %d, %u\n", isAlreadGetData, timestamp);
#else
		struct pt_regs regs;
		isAlreadGetData++;
		printk("Dio not Released rec times= %d\n", isAlreadGetData);
		asm_do_IRQ( ts_data->irq, &regs );
#endif
	}
}



static int mcs6000_ts_input_init(struct mcs6000_ts_data *data)
{
	struct input_dev *input_dev;
	int ret = 0;

	printk("########################################\n");
	printk("%s...\n",__func__);
	printk("########################################\n");
	INIT_WORK(&data->ts_event_work, mcs6000_ts_irq_worker);
	INIT_WORK(&data->ts_OP_MODE_change, mcs6000_ts_opmode_change_worker);
#ifdef SEND_RELEASE_EVENT
	INIT_WORK(&data->ts_release_event_work, mcs6000_ts_release_worker);
#endif

	data->input_dev = input_allocate_device();
	if (data->input_dev == NULL) {
		ret = -ENOMEM;
		goto err_input;
	}

	input_dev = data->input_dev;
	input_dev->name = "MELPAS MCS-6000 Touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &data->client->dev;
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(ABS_MISC, input_dev->absbit);

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	input_set_abs_params(input_dev, ABS_X, 0, MCS6000_MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, MCS6000_MAX_YC, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, 256, 0, 0);

	ret = input_register_device(data->input_dev);
	if (ret < 0)
		goto err_register;

	input_set_drvdata(input_dev, data);

	TSSC_wq = create_singlethread_workqueue("TSSC_wq");
	if (!TSSC_wq) {
		printk("!TSSC_wq created failed\r\n");
		goto err_irq;
	}

	ret = request_irq(data->irq, mcs6000_ts_interrupt, IRQF_TRIGGER_LOW, "mcs6000_ts_input", NULL);
	if (ret < 0) {
		dev_err(&data->client->dev, "Failed to register interrupt\n");
		goto err_irq;
	}

	return 0;
err_irq:
	input_unregister_device(data->input_dev);
	data->input_dev = NULL;
err_register:
	input_free_device(data->input_dev);
err_input:
	return ret;
}

#if 0
static void mcs6000_ts_phys_init(struct mcs6000_ts_data *data)
{
	struct i2c_client *client = data->client;

	/* Touch reset & sleep mode */
	mcs6000_i2c_write(client, MCS6000_TS_OP_MODE,
		/*RESET_EXT_SOFT |*/ OP_MODE_SLEEP);

	/* Touch active mode & 80 report rate */
	mcs6000_i2c_write(data->client, MCS6000_TS_OP_MODE,
			OP_MODE_ACTIVE/* | GESTURE_ENABLE | REPORT_RATE_80*/);
}
#endif

static int __devinit mcs6000_ts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	struct mcs6000_ts_data *data;
	int ret = 0;
	int rc;
	struct vreg *vreg_ruim;
	int err;
    bool bAtemlIsExist = false;


    //Henry_lin, 20101115, [BugID 1013] MELFAS TP f/w 0x80 cause TP register read fail or enter download mode fail.
    bAtemlIsExist = TPiIsExist();
    //Printhh("[%s] bAtemlIsExist = %d\n", __FUNCTION__, (int)bAtemlIsExist);
    if(bAtemlIsExist)
    {
        PrintTip("[%s] Atmel TP exist, stop melfas detection...\n", __FUNCTION__);
        ret = -ENODEV;
        goto exit_allocate_fail;
    }
    PrintTip("[%s] Atmel TP not exist, melfas detecting...\n", __FUNCTION__);
    //Henry_lin

	printk("########################################\n");
	printk("%s...\n",__func__);
	printk("########################################\n");
	data = kzalloc(sizeof(struct mcs6000_ts_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit_allocate_fail;
	}

#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
	IS_DVT_1_3 = DVT_1_3;
	if (HW_ID4==1)
		TOUCH_GPIO = 19;
#endif
	printk("%s - IS_DVT_1_3 = %d \n",__FUNCTION__,IS_DVT_1_3);
	if (IS_DVT_1_3) {
		printk("gpio_tlmm_config - for gpio TS_CE = %d\n",TS_CE);
		if (gpio_tlmm_config(GPIO_CFG(TS_CE, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE)) {
			printk(KERN_ERR "%s: Err: Config GPIO-84 \n", __func__);
		}
	
		if (gpio_request(TS_CE, "ts_ce"))
			printk(KERN_ERR "failed to request gpio ts_ce\n");
		gpio_configure(TS_CE,GPIOF_DRIVE_OUTPUT);
		gpio_direction_output(TS_CE,0);
		printk("%s: TS_CE = %d\n",__func__,gpio_get_value(TS_CE) );
	}

	rc = gpio_request(TOUCH_GPIO,"touch_int");
	if (rc < 0) {
		printk("fail to request gpio for touch_int! error = %d\n",rc);
		goto exit;
	}
	gpio_direction_output(TOUCH_GPIO, 1);
	printk("%s: TOUCH_GPIO = %d\n",__func__,gpio_get_value(TOUCH_GPIO) );

	mdelay(20);
	if (!IS_DVT_1_3) {
	    vreg_ruim = vreg_get(NULL, "ruim");
		printk("%s HW VERSION : before DVT_1_3\n",__func__);
		vreg_disable(vreg_ruim);
		mdelay(20);
		vreg_set_level(vreg_ruim,2600);
		vreg_enable(vreg_ruim);
		mdelay(20);
		vreg_disable(vreg_ruim);
		mdelay(20);
		vreg_enable(vreg_ruim);
	} else if (IS_DVT_1_3) {
		gpio_direction_output(TS_CE,1);
		printk("%s: TS_CE = %d\n",__func__,gpio_get_value(TS_CE) );
	}
	mdelay(200);

	//vreg_ruim = vreg_get(NULL, "ruim");
	//vreg_disable(vreg_ruim);

	//mdelay(200);

	//vreg_ruim = vreg_get(NULL, "ruim");
	//vreg_set_level(vreg_ruim,2600);
	//vreg_enable(vreg_ruim);

	//rc = gpio_get_value(TOUCH_GPIO);
	//printk("##### TOUCH_GPIO 2 = %d\n #####",rc);

	data->client = client;
	data->platform_data = client->dev.platform_data;
	data->irq = MSM_GPIO_TO_INT(TOUCH_GPIO);//client->irq;
	ts_data = data;
	atomic_set(&data->irq_disable, 1);

	i2c_set_clientdata(client, data);
	//setup_timer(&timer, ts_timer, (unsigned long)data);

	//if(IS_DVT_1_3) {
		//mcs6000_i2c_write(client, MCS6000_TS_OP_MODE, 0x13);
		//mdelay(400);
		//mcs6000_i2c_write(client, MCS6000_TS_OP_MODE, 0x12);
		//mdelay(200);
		//printk("%s HW VERSION : after DVT_1_3\n",__func__);
	//}

	err = mcs6000_i2c_read(client,MCS6000_FW_VERSION,&tp_fw_version,1);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		//Henry_lin, 20101115, [BugID 1013] MELFAS TP f/w 0x80 cause TP register read fail or enter download mode fail.
		#if 0
		goto exit_free;
		#else
		//do nothing, set f/w version to zero. force to do download f/w
		tp_fw_version = 0;
		#endif
//		return 0;
	}
	printk("%s FW_VERSION = 0x%x\n",__func__,tp_fw_version);
	
	err = mcs6000_i2c_read(client,MCS6000_SN_VERSION,&tp_sensor_reversion,1);
	if (err < 0) {
		dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
		//Henry_lin, 20101115, [BugID 1013] MELFAS TP f/w 0x80 cause TP register read fail or enter download mode fail.
		#if 0
		goto exit_free;
		#endif
//		return 0;
	}
	printk("%s SENSOR_REVERSION = 0x%x\n",__func__,tp_sensor_reversion);

	if (IS_DVT_1_3 && (tp_fw_version < CUR_TP_VER)) {
		PrintTip("[%s] TP chip fw version = %#x, driver provided fw = %#x\n", __FUNCTION__, tp_fw_version, CUR_TP_VER);
		printk("################DL MODE START################\n");
		gpio_direction_output(TS_CE,0);
		mcs6000_dl_process(mcs6k_dl_data);
		printk("################DL MODE FINISH################\n");
		gpio_direction_output(TS_CE,0);
		mdelay(200);
		gpio_direction_output(TS_CE,1);
		mdelay(200);
//		mcs6000_i2c_write(client, MCS6000_TS_OP_MODE, 0x13);
//		mdelay(400);
//		mcs6000_i2c_write(client, MCS6000_TS_OP_MODE, 0x12);
//		mdelay(200);

		err = mcs6000_i2c_read(client,MCS6000_FW_VERSION,&tp_fw_version,1);
		if (err < 0) {
			dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
			goto exit_free;			
//			return 0;
		}
		printk("AFTER DL : %s FW_VERSION = 0x%x\n",__func__,tp_fw_version);

		err = mcs6000_i2c_read(client,MCS6000_SN_VERSION,&tp_sensor_reversion,1);
		if (err < 0) {
			dev_err(&client->dev, "%s, err[%d]\n", __func__, err);
			goto exit_free;			
//			return 0;
		}
		printk("AFTER DL : %s SENSOR_REVERSION = 0x%x\n",__func__,tp_sensor_reversion);
	}

	mdelay(20);
	rc = gpio_direction_input(TOUCH_GPIO);
	if (rc < 0)
		printk("fail to set direction of touch_int gpio! error = %d\n",rc);

	printk("%s: TOUCH_GPIO = %d\n",__func__,gpio_get_value(TOUCH_GPIO) );

	ret = mcs6000_ts_input_init(data);
	if (ret)
		goto exit_free;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts_early.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	ts_early.suspend = ts_early_suspend;
	ts_early.resume = ts_later_resume;
	register_early_suspend(&ts_early);
#endif
	//Dio for UI use,	
	init_tp_data_proc();

#ifdef CONFIG_SLATE_TEST
	ts_input_dev = data->input_dev;
#endif
	init_timer(&ts_timer);
	ts_timer.function = ts_timeout;

	//Dio
	//hrtimer_init(&test_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	//test_timer.function = mcs6000_ts_timer_func;
	//hrtimer_start(&test_timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;

exit_free:
	input_free_device(data->input_dev);
	kfree(data);
	i2c_set_clientdata(client, NULL);
	#if 0   //henry move to exit_allocate_fail
	i2c_set_clientdata(mcs6k_dl_data->client, NULL);
	kfree(mcs6k_dl_data);
	#endif
exit_allocate_fail:
	#if 1   // free dl allocated resource
	i2c_set_clientdata(mcs6k_dl_data->client, NULL);
	kfree(mcs6k_dl_data);
	#endif
exit:
	return ret;
}

static int __devexit mcs6000_ts_remove(struct i2c_client *client)
{
	struct mcs6000_ts_data *data = i2c_get_clientdata(client);

	cancel_work_sync(&data->ts_event_work);

	/*
	 * If work indeed has been cancelled, disable_irq() will have been left
	 * unbalanced from mcs6000_ts_interrupt().
	 */
	while (atomic_dec_return(&data->irq_disable) > 0)
		disable_irq(data->irq);

	input_unregister_device(data->input_dev); 
	free_irq(data->irq, data);
	kfree(data);

	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
void ts_early_suspend(struct early_suspend *h)
{
	struct vreg *vreg_ruim; 

	atomic_dec(&ts_data->irq_disable);
	disable_irq(ts_data->irq);
	del_timer(&ts_timer);
	flush_workqueue(TSSC_wq);

	set_bit(1, &suspending);

	isReleased =0;
	isAlreadGetData = 0; // use to tag if we already get TP data.
	tp_down = 0;
	UI_OP_MODE=UI_OP_MODE_ACTIVE;

	if (!IS_DVT_1_3) {
		vreg_ruim = vreg_get(NULL, "ruim");
		printk("%s HW VERSION : before DVT_1_3\n",__func__);
		vreg_disable(vreg_ruim);
	} else if (IS_DVT_1_3) {
		gpio_direction_output(TS_CE,0);
		printk("%s: TS_CE = %d\n",__func__,gpio_get_value(TS_CE) );
	}

	gpio_direction_output(TOUCH_GPIO,0);
	printk("%s: TOUCH_GPIO = %d\n",__func__,gpio_get_value(TOUCH_GPIO) );

	printk("%s...\n",__func__);
}

void ts_later_resume(struct early_suspend *h)
{
	struct vreg *vreg_ruim;
 
	gpio_direction_output(TOUCH_GPIO,1);
	printk("%s: TOUCH_GPIO = %d\n",__func__,gpio_get_value(TOUCH_GPIO) );

	mdelay(20);

	if (!IS_DVT_1_3) {
		vreg_ruim = vreg_get(NULL, "ruim");
		vreg_set_level(vreg_ruim,2600);
		vreg_enable(vreg_ruim);
		printk("%s HW VERSION : before DVT_1_3\n",__func__);
	} else if (IS_DVT_1_3) {
		gpio_direction_output(TS_CE,1);
		printk("%s: TS_CE = %d\n",__func__,gpio_get_value(TS_CE) );
	}

	mdelay(50);

	gpio_direction_input(TOUCH_GPIO);
	printk("%s: TOUCH_GPIO = %d\n",__func__,gpio_get_value(TOUCH_GPIO) );

    
	clear_bit(1, &suspending);
	enable_irq(ts_data->irq);
	atomic_inc(&ts_data->irq_disable);

	printk("%s...\n",__func__);
}
#endif


static const struct i2c_device_id mcs6000_ts_id[] = {
	{ "mcs6000_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mcs6000_ts_id);

static struct i2c_driver mcs6000_ts_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name = "mcs6000_ts",
	},
	.probe		= mcs6000_ts_probe,
	.remove		= __devexit_p(mcs6000_ts_remove),
	.id_table	= mcs6000_ts_id,
};


static int __init mcs6000_ts_init(void)
{	
	i2c_add_driver(&mcs6000_dl_driver);

	return i2c_add_driver(&mcs6000_ts_driver);
}

static void __exit mcs6000_ts_exit(void)
{
	i2c_del_driver(&mcs6000_dl_driver);
	i2c_del_driver(&mcs6000_ts_driver);
}

module_init(mcs6000_ts_init);
module_exit(mcs6000_ts_exit);

/* Module information */
MODULE_AUTHOR("Albert Wu");
MODULE_DESCRIPTION("Touch screen driver for CCI KB60");
MODULE_LICENSE("GPL");
