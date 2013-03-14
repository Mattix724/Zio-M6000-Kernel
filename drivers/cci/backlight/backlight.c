#include <linux/leds.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <../drivers/video/msm/mddihost.h>
#include <../drivers/video/msm/mddihosti.h>
#include <mach/msm_battery.h>
/*(S)KYOCERA_NEW SPECIFICATION*/
#include <mach/mpp.h>
/*(E)KYOCERA_NEW SPECIFICATION*/

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static int backlight_power_state=1;//0 for OFF 1 for ON
static int current_backlight_val=0;
#endif

#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
extern int backlight_val;
#endif

/*(S)KYOCERA_NEW SPECIFICATION*/
extern int KeyBackLightLedOn_flag;/* 1:POWER ON, 0:POWER OFF */
struct mpp *key_led_switch_mpp9 = NULL;
/*(E)KYOCERA_NEW SPECIFICATION*/

//weijiun_ling
#ifdef NOTIFY_BATT__FUNCTION
int prev_level = 0;

static void Notify_Batt_drv(int value)
{
    int level = value / 64;

    if (prev_level != level)
    {
        cci_batt_device_status_update(0xE0,0); 
        if (level > 2)
            cci_batt_device_status_update(CCI_BATT_DEVICE_ON_LCD_BACKLIGHT_150,1);
        else if (level > 1)
            cci_batt_device_status_update(CCI_BATT_DEVICE_ON_LCD_BACKLIGHT_100,1);  
        else if (level > 0)
            cci_batt_device_status_update(CCI_BATT_DEVICE_ON_LCD_BACKLIGHT_50,1);
        else 
            cci_batt_device_status_update(0xE0,0); 
	  prev_level = level;
    }
}
#endif

static void set_lcd_backlight(struct led_classdev *led_dev, enum led_brightness value)
{
/*(S)KYOCERA_NEW SPECIFICATION*/
    static u8 is_key_led_5mA = false;

	key_led_switch_mpp9 = mpp_get(NULL, "mpp9");
/*(E)KYOCERA_NEW SPECIFICATION*/
	//range 0-255
	if(value > 255)
		value = 255;
	if(value < 1)
		value = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
       current_backlight_val=value;
#endif

#ifdef CONFIG_FB_MSM_MDDI_CCI_TPO_WVGA
	backlight_val = (255-value);
#endif
        //printk("brightness value = %d\n",value);

#ifdef NOTIFY_BATT__FUNCTION
        //weijiun_ling
        Notify_Batt_drv(value);
#endif

        if(backlight_power_state)
        {
        //printk("brightness value Setted \n");
	mddi_queue_register_write(0x6A18 ,(255-value), TRUE, 0); //write mddi register set brightness value
/*(S)KYOCERA_NEW SPECIFICATION*/
        if(KeyBackLightLedOn_flag)
        {
            if ( value == 17 )
            {
                (void)rpc_mpp_config_i_sink(key_led_switch_mpp9, PM_MPP__I_SINK__LEVEL_5mA, PM_MPP__I_SINK__SWITCH_ENA);
                printk("[KEY LED]key backlight LED set 5mA!! in %s()\n", __func__);
                is_key_led_5mA = true;
            }
            else if ( value > 17 )
            {
                if( is_key_led_5mA )
                {
                    (void)rpc_mpp_config_i_sink(key_led_switch_mpp9, PM_MPP__I_SINK__LEVEL_10mA, PM_MPP__I_SINK__SWITCH_ENA);
                    printk("[KEY LED]key backlight LED set 10mA!! in %s()\n", __func__);
                    is_key_led_5mA = false;
                }
            }
        }
/*(E)KYOCERA_NEW SPECIFICATION*/
        }
}


static struct led_classdev lcd_backlight = {
	.name = "bkl",
	.brightness_set = set_lcd_backlight,
	.default_trigger = "charger",
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend kb60_backlight_suspend;
void kb60_backlight_early_suspend(struct early_suspend *h)
{
  printk("%s:\n", __func__);
  backlight_power_state=0;
}

void kb60_backlight_later_resume(struct early_suspend *h)
{
  printk("%s:\n", __func__);
  backlight_power_state=1;
}

void backlight_register_driver(void)
{
  kb60_backlight_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB -3;
  kb60_backlight_suspend.suspend = kb60_backlight_early_suspend;
  kb60_backlight_suspend.resume = kb60_backlight_later_resume;
  register_early_suspend(&kb60_backlight_suspend);
}

static int backlight_suspend(struct platform_device *dev, pm_message_t state)
{
  printk("%s:\n", __func__);
  set_lcd_backlight(&lcd_backlight, 0);
  backlight_power_state=0;
  return 0;
}

static struct platform_driver msm_backlight_driver = {
	.suspend = backlight_suspend,
        .driver = {
		   .name = "msm-backlight",
		   .owner = THIS_MODULE,
		   },
};
#endif

static int __init cci_kb60_backlight_init(void)
{
	printk("[BACKLIGHT]: module init.\n");
	led_classdev_register(NULL, &lcd_backlight);
        #ifdef CONFIG_HAS_EARLYSUSPEND
        {
            int rc;
            backlight_register_driver();
            rc = platform_driver_register(&msm_backlight_driver);
            if (rc < 0)
            {
                printk(KERN_ERR "%s: platform_driver_register failed for "
                "backlight driver. rc = %d\n", __func__, rc);
                return rc;
            }
        }
        #endif
	mddi_queue_register_write(0x5307  ,0x01, TRUE, 0);; //write mddi register for Set The Total Dimming Steps For Still-Mode of CABC to 4 steps
	mddi_queue_register_write(0x5308  ,0x01, TRUE, 0);; //write mddi register for Set The Total Dimming Steps For Moving-Mode of CABC  to 4 steps
	return 0;
}


static void __exit cci_kb60_backlight_exit(void)
{
	printk("[BACKLIGHT]: module exit.\n");
	led_classdev_unregister(&lcd_backlight);
}


module_init(cci_kb60_backlight_init);
module_exit(cci_kb60_backlight_exit);

MODULE_DESCRIPTION("Backlight controller on KB60");
MODULE_AUTHOR("Aaron Chen");
MODULE_LICENSE("GPL v2");
