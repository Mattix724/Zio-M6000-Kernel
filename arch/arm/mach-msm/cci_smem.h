#ifndef _ARCH_ARM_MACH_MSM_CCI_SEME_H_
#define _ARCH_ARM_MACH_MSM_CCI_SMEM_H_

/* Charging Status*/
typedef enum {  
    CCI_CHG_STATE_NONE = 0x00,      /* no charger */
    CCI_CHG_STATE_CHARGING,         /* battery is charging */
    CCI_CHG_STATE_COMPLETE,         /* charging complete */
    CCI_CHG_STATE_ERR_TEMP_STOP_CHGARGING,	/* battery too hot or cold and stop charging */
    CCI_CHG_STATE_ERR_TEMP_STILL_CHGARGING,	/* battery too hot or cold but still charging */
    CCI_CHG_STATE_ERR_CHARGING,     /* charging abnormal, use weak current to charge battery. eg. weak battery, bad charger, weak charger */
    CCI_CHG_STATE_ERR_STOP,         /* charging abnormal, stop charging battery. eg. bad battery */
    CCI_CHG_STATE_INVALID = 0xFFFFFFFF    /* invalid charging state */

}cci_charging_state;

typedef enum{
	CCI_VALID_BATTERY,
	CCI_INVALID_BATTERY,
	CCI_SKIP_BATTERY_CHECK,
     //#ifdef CONFIG_CCI_KB60
	CCI_BATTERY_CRITICAL,
	CCI_BATTERY_POWERDOWN,
     //#endif
	CCI_BATTERY_INFO_INVALID = 0xFFFFFFFF
}cci_battery_info_type; 

// CCI Modem_L1 Cloud Add_Begin
typedef enum {
	CCI_BATT_TEMP_OK,						// Battery temperature is ok
	CCI_BATT_TEMP_ERROR_LV0,				// Battery temperature is error, stop charging
	CCI_BATT_TEMP_ERROR_LV1,				// Battery temperature is error, set charging current to 500mA
	CCI_BATT_TEMP_ERROR_LV2,				// Battery temperature is error, set charging current to 100mA
	CCI_BATT_TEMP_INVALID = 0xFFFFFFFF	// Invalid value
}cci_battery_temperature_info_type;
// CCI Modem_L1 Cloud Add_End

typedef enum{
    CCI_SET_AC,                             //  set AC current, 1A
    CCI_SET_USB,                           //  set USB current, 500mA
    CCI_SET_KITL,					//  set USB current 400mA for KIPL test		//PVCS ID:modem_7k TK671 [1/8]  AV30 V0.0.28  Pierce 090706
    CCI_SET_RE_EMULATION,		//  set USB current 100mA for re-emulation	//PVCS ID:modem_7k TK671
    CCI_SET_INVALID = 0xFFFFFFFF           //  invalid value
}cci_set_power_source_type;

typedef enum{
	CCI_FACTORY_CHARGING_COMPLETE,
	CCI_FACTORY_CHARGING_UN_COMPLETE,
	CCI_FACTORY_CHARGING_INFO_INVALID = 0xFFFFFFFF
}cci_factory_charging_info_type;

/*Bootup Condition*/
typedef enum
{
 	CCI_DOWNLOAD_ANDROID_IMAGE,
	CCI_UN_DOWNLOAD_ANDROID_IMAGE,
	CCI_DOWNLODA_ANDROID_IMAGE_UNKONW = 0xFFFFFFFF
	
} cci_android_image_download_check_config;

/*Modem Specified Mode*/
typedef enum {
	CCI_MODEM_NORMAL_POWER_ON_MODE,
	CCI_MODEM_CHARGING_ONLY_MODE,
	CCI_MODEM_FTM_MODE,
	CCI_MODEM_ARM9_FTM_MODE,
	CCI_MODEM_DOWNLOAD_MODE,
	CCI_MODEM_EFS_FORMAT_MODE,
	CCI_MODEM_FTM_WIFI_MODE,
	CCI_MODEM_DL_BATT_PROFILE_MODE,
	CCI_MODEM_CFC_TEST_MODE,
   	CCI_MODEM_ANDROID_DL_MODE,
    	CCI_FOTA_AMSS_DL_MODE,
	CCI_MODEM_UNKNOW_MODE = 0xFFFFFFFF
} cci_modem_boot_mode_config;



/*Power-On-Down Event*/
typedef enum {
	CCI_POWER_ON_DOWN_FROM_POWER_KEY=1,
	CCI_POWER_ON_DOWN_FROM_CHARGER,
	CCI_POWER_ON_DOWN_FROM_POWER_CUT,
	CCI_POWER_ON_DOWN_FROM_SW_RESET,
	CCI_POWER_ON_DOWN_FROM_HW_RESET,
	CCI_POWER_ON_DOWN_FROM_DOWNLOAD,
	CCI_POWER_ON_DOWN_FROM_FTM,
	CCI_POWER_ON_DOWN_FROM_EFS_FORMAT,
	CCI_POWER_ON_DOWN_FROM_ARM9_FATAL_ERROR,
	CCI_POWER_ON_DOWN_FROM_WDOG,
	CCI_POWER_ON_DOWN_FROM_ARM9_FTM,
	CCI_POWER_ON_DOWN_FROM_CFC_TEST_MODE,
	CCI_POWER_ON_DOWN_FROM_BATT_REMOVE,
	CCI_POWER_ON_DOWN_FROM_PMIC_RTC,
	CCI_POWER_ON_DOWN_FROM_ANDROID_DL,
	CCI_POWER_ON_DOWN_FROM_FOTA_AMSS_DL,
	CCI_POWER_ON_DOWN_FROM_OTHER,
	CCI_POWER_ON_DOWN_FROM_UNKOWN,
	CCI_POWER_ON_DOWN_FROM_INTALL_MODEM_FLEX_ONLY,
	CCI_POWER_ON_DOWN_FROM_MAX=20,		
	CCI_POWER_ON_DOWN_FROM_INVALID = 0xFFFFFFFF
} cci_power_on_down_event_config;





/*ANDROID specified mode*/
typedef enum {
    CCI_ANDROID_UNLOAD_MODE,
    CCI_ANDROID_NORMAL_MODE,
    CCI_ANDROID_CHARGING_ONLY_MODE,
    CCI_ANDROID_FTM_WIFI_MODE,
    CCI_ANDROID_CFC_TEST_MODE,
    CCI_ANDROID_DL_MODE,
//#ifdef CONFIG_CCI_KB60
    CCI_ANDROID_FOTA_AMSS_MODE,
//#endif
    CCI_ANDROID_UNKNOW_MODE = 0xFFFFFFFF
} cci_android_boot_mode_config;



typedef enum {
	CCI_HW_RF_BAND_IS_EU 			= 0x1,		/* Bit 0~3 are for RF band */
	CCI_HW_RF_BAND_IS_US 			= 0x2,
	CCI_HW_BOARD_VERSION_BEFORE	= 0x10,	/* Bit 4~7 are for board version */
	CCI_HW_BOARD_VERSION_DVT1_4	= 0x20,
	CCI_HW_BOARD_VERSION_1       = 0x10,                    //HW ID0 GPIO83 detect high (EVT)
  CCI_HW_BOARD_VERSION_2       = 0x20,                    //HW ID0 GPIO83 detect low	(DVT1) and HW ID1 GPIO(N/A) detect high
  CCI_HW_BOARD_VERSION_3       = 0x30,                    //HW ID0 GPIO83 detect low         and HW ID1 GPIO(N/A) detect low  HW ID2 GPIO(N/A) detect high 
  CCI_HW_BOARD_VERSION_4       = 0x40,                    //HW ID0 GPIO83 detect low 				 and HW ID1 GPIO(N/A) detect low  HW ID2 GPIO(N/A) detect low
  CCI_HW_BOARD_VERSION_5      = 0x50,		//[Joseph] KB62 HW ID
  CCI_HW_BOARD_VERSION_NONE    = 0xF0,          //no HW ID GPIO detect circuit
  	CCI_HW_LQ86_EVT					= 0x000,		//LQ86 EVT1 TK337 LQ86 090601 V1.02
  	CCI_HW_LQ86_DVT1_1				= 0x110,		//LQ86 DVT1-1 GPIO90 Low TK337 LQ86 090601 V1.02
  	CCI_HW_LQ86_DVT1_2				= 0x210,		//LQ86 DVT1-2 GPIO90 High TK337 LQ86 090601 V1.02
  	CCI_HW_LQ86_DVT2_1				= 0x120,		//LQ86 DVT2-1 TK337 LQ86 090601 V1.02
  	CCI_HW_LQ86_DVT2_2				= 0x220,		//LQ86 DVT2-2 TK337 LQ86 090601 V1.02
	CCI_HW_VERSION_INFO_INVALID = 0xFFFFFFFF
} cci_hw_version_info_type;

#if defined(BUILD_AB60)
typedef enum {
   	EMU= 0,
  	EVT0= 0x5,
   	EVT1= 0x6,
   	EVT2= 0x7,
   	DVT=0xA,
   	PVT= 0xD,
   	HW_ID_INVALID = 0xFF
} cci_hw_id_type;

typedef enum {
	WCDMA_US_125=0,
	WCDMA_EU_128=0x10,
	EDGE=0x20,
	BAND_ID_INVALID = 0xFF
} cci_band_id_type;

#endif

// 081017 L1 Leon add for download incomplete: START 
typedef enum{
	CCI_DM_SUCEESS,
	CCI_DM_QCSBLHD_FAIL,
	CCI_DM_QCSBL_FAIL,
	CCI_DM_OEMSBL_FAIL,
	CCI_DM_AMSS_FAIL,
	CCI_DM_APPSBL_FAIL,
	CCI_DM_APPS_FAIL,
	CCI_DM_FLASH_BIN_FAIL,
	CCI_DM_FLEX_FAIL,
    	CCI_DM_BOOT_FAIL,
    	CCI_DM_SYSTEM_FAIL,
    	CCI_DM_USERDATA_FAIL,
    	CCI_DM_RECOVERY_FAIL,
	CCI_DM_INVALID = 0xFFFFFFFF
}cci_download_check_condition;
// 081017 L1 Leon add for download incomplete: END

// CCI Modem_L1 Chihyu Add Begin 20090629
typedef enum {

    CCI_APPS_RPC_NOT_READY = 0x00,      	/* Android RPC Not Ready */
    CCI_APPS_RPC_READY, 	 		/* Android RPC Ready */		
    CCI_APPS_RPC_INVALID = 0xFFFFFFFF    	/* invalid RPC state */

}cci_rpc_state;
// CCI Modem_L1 Chihyu 20090629 Add End

typedef enum {

    CCI_CHG_SET_IDLE_MODE,
    CCI_CHG_SET_CC_MODE,
    CCI_CHG_SET_CV_MODE,     
    CCI_CHG_SET_MODE_INVALID = 0xFFFFFFFF            /* invalid RPC state */

}cci_chg_current_state;

typedef struct {
	cci_android_image_download_check_config	cci_android_image_download_check_flag;
	cci_power_on_down_event_config			cci_power_on_event;
	cci_power_on_down_event_config			cci_power_down_event;
	cci_modem_boot_mode_config                      cci_modem_boot_mode;
	cci_android_boot_mode_config     		cci_android_boot_mode;
	cci_battery_info_type				cci_battery_info;
	cci_hw_version_info_type			cci_hw_version_info;
	char						oemsbl_version[10];
	char 						modem_version[10];
	
	char						os_version[10];
	char						language_id[10];
	cci_rpc_state 					cci_android_rpc_state;
	cci_download_check_condition 			download_check_flag; // 081017 L1 Leon add for download incomplete
	cci_charging_state 				cci_modem_charging_state;
	cci_factory_charging_info_type			cci_factory_charging_check;
	unsigned short					cci_gauge_ic_fw_version;		//TK316  V0.019 #14# debug information for gauge version.
	unsigned long					cci_gauge_ic_profile_version;	//TK316  V0.019 #14# debug information for gauge version.
        cci_chg_current_state                           cci_set_chg_state;
	unsigned short					cci_fota_amss_progress;
	char							CCI_IMEI_Num[15];           //[Joseph] IMEI NVITEM
	char							CCI_NVITEM_MEID_READY;       ////[Joseph] IMEI NVITEM ready flag (1 --> readdy to read)
}cci_smem_value_t;

#endif
