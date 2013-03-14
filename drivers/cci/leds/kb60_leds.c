#include <linux/leds.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/ctype.h>
#include <mach/mpp.h>
#include <linux/timer.h>   
#include <linux/hrtimer.h> 
#include <asm/gpio.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include "kb60_leds.h"


#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

struct hrtimer led_timer; 
struct mpp *led_switch_mpp9 = NULL; 
static struct work_struct  LEDMon_work;  
int keylight_flag = 0; 
int IncomingCall_flag = 0; 
/* wayne add for tracker 8299*/
int KeepingOn_flag = 0; 
/* wayne add for tracker 8633*/
int InCallScreen_flag = 0; 
int TimerCrl_flag = 0; 
                   
/*(S)KYOCERA_NEW SPECIFICATION*/
#define F_KEYLED_OFF    0
#define F_KEYLED_ON     1
int KeyBackLightLedOn_flag = F_KEYLED_OFF;
/*(E)KYOCERA_NEW SPECIFICATION*/

static DEFINE_MUTEX(keylight_mutex);

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend kb60_led_suspend;
#endif


int disable_mpp9(int flag_control){

        int rc = 0;
	printk("%s\n",__FUNCTION__);
        mutex_lock(&keylight_mutex);
       
        led_switch_mpp9 = mpp_get(NULL, "mpp9"); //NULL is not good, just test now...

        rc = rpc_mpp_config_i_sink(led_switch_mpp9, PM_MPP__I_SINK__LEVEL_20mA, PM_MPP__I_SINK__SWITCH_DIS); //0 means off

	KeyBackLightLedOn_flag = F_KEYLED_OFF;/*KYONCERA_NEW SPECIFICATION*/
        if (rc!=0)
              printk(KERN_ERR"%s: mpp_config_i_sink mpp9 failed (%d)\n",__func__, rc);

        if(flag_control == 1){
            keylight_flag = 0;
            }else if(flag_control == 0){
	             keylight_flag = 0 ;
        	     IncomingCall_flag = 0;
	             }
        
        mutex_unlock(&keylight_mutex);
	
	return rc;
}


void KeypadLed_shutdown(struct work_struct *work){                 

	int rc = 0;
        rc = disable_mpp9(0); //james@cci , both flag to 0
}                                                    


static void kb60_android_key_led_turn_off(void)
{
	schedule_work(&LEDMon_work);            
}


void set_red(struct led_classdev *led_dev, enum led_brightness v)
{
        int rc = 0;

	mutex_lock(&keylight_mutex);	

	rc = rpc_mpp_config_led_state(v, PM_MPP__LED_STATE__COLOR_RED); 

	if (rc!=0)
		printk(KERN_ERR"%s: mpp_config_led_state red activated failed (%d)\n",__func__, rc);

	mutex_unlock(&keylight_mutex);
}

static struct led_classdev redled = {
	.name = "red",
	.brightness_set = set_red,
};

void set_green(struct led_classdev *led_dev, enum led_brightness value)
{
	int rc = 0;

	mutex_lock(&keylight_mutex);

	rc = rpc_mpp_config_led_state(value, PM_MPP__LED_STATE__COLOR_GREEN); 
	if (rc!=0)
		printk(KERN_ERR"%s: mpp_config_led_state green activated failed (%d)\n",__func__, rc);

	mutex_unlock(&keylight_mutex);
}

static struct led_classdev greenled = {
	.name = "green",
	.brightness_set = set_green,
	.default_trigger = "null",
};

void set_key(struct led_classdev *led_dev, enum led_brightness value)
{
	int rc = 0;

	mutex_lock(&keylight_mutex);

       	led_switch_mpp9 = mpp_get(NULL, "mpp9"); //NULL is not good, just test now...

	if(value)  //james@cci , value > 0 , 1 or 2 , start timer to turn off key light for key pressed and incoming call case?
       	{
            if ((value == 5)||(value == 6))
               {
                 if (value == 5)
                     InCallScreen_flag = 1;
                 else
                     InCallScreen_flag = 0;
               }	
            else if(keylight_flag)
          	{
                        if (!TimerCrl_flag) 
                    	       hrtimer_cancel(&led_timer); 

                        if ((IncomingCall_flag == 1)||(value == 2)) {     //james@cci, value == 2 means turn off key light? at incomingcall case??
               			if (!KeepingOn_flag) {
                                    hrtimer_start(&led_timer,ktime_set(32,0), HRTIMER_MODE_REL);
		               	    IncomingCall_flag = 1; //james@cci why still set it IncomingCall_flag as 1?? not 0 ???
                                  	}  
			}
           		else if (value == 1) {
                                if (!KeepingOn_flag) 
              			hrtimer_start(&led_timer,ktime_set(8,0), HRTIMER_MODE_REL);
                                //KeepingOn_flag = 0;
			}
                        else if (value == 3){        //wayne, value = 3 for keylight keeping on.
           		        KeepingOn_flag = 1;
                                TimerCrl_flag = 1;
           		}
                          else {
                                hrtimer_start(&led_timer,ktime_set(8,0), HRTIMER_MODE_REL);
           		        KeepingOn_flag = 0;
                                TimerCrl_flag = 0;
           		}   
                }
          	else 
          	{
           	   if(!InCallScreen_flag)	
                    {
           		rc = rpc_mpp_config_i_sink(led_switch_mpp9, PM_MPP__I_SINK__LEVEL_10mA, 1); //james@cci , 1 means ?? turn on ?

			KeyBackLightLedOn_flag = F_KEYLED_ON;/*KYONCERA_NEW SPECIFICATION*/
	   		if (rc!=0)
	      			printk(KERN_ERR"%s: mpp_config_i_sink mpp9 failed (%d)\n",__func__, rc);

           		if (value == 1){	//james@cci , value == 1 means turn off key light ?? at key pressed case?
           		        if (!KeepingOn_flag) 
                                  hrtimer_start(&led_timer,ktime_set(8,0), HRTIMER_MODE_REL);
			}
           		else if (value == 2){
           			if (!KeepingOn_flag) {
                                  hrtimer_start(&led_timer,ktime_set(32,0), HRTIMER_MODE_REL);
           			  IncomingCall_flag = 1;
                                 } 
           		}
                        else if (value == 3){
           		        //hrtimer_start(&led_timer,ktime_set(1,0), HRTIMER_MODE_REL);
                                KeepingOn_flag = 1;
                                TimerCrl_flag = 1;
           		}
                        else {
           		        hrtimer_start(&led_timer,ktime_set(8,0), HRTIMER_MODE_REL);
                                KeepingOn_flag = 0;
                                TimerCrl_flag = 0;
           		}
	           	keylight_flag = 1;
          	}
       	}
       	}
    	else
       	{
        	if (!KeepingOn_flag) {
                if (IncomingCall_flag == 1)	//james@cci , why only check incoming call flag ?? not with key flag ?? !!!!!!!!!! and timer???
			hrtimer_cancel(&led_timer);

	        rc = rpc_mpp_config_i_sink(led_switch_mpp9, PM_MPP__I_SINK__LEVEL_10mA, value); //james@cci if value <=0 . ex . -1 ?? how about rpc call result??
		KeyBackLightLedOn_flag = F_KEYLED_OFF;/*KYONCERA_NEW SPECIFICATION*/
	  	if (rc!=0)
			printk(KERN_ERR"%s: mpp_config_i_sink mpp9 failed (%d)\n",__func__, rc);

		keylight_flag = 0;
           	IncomingCall_flag = 0;
                }
	}
	mutex_unlock(&keylight_mutex);
}

static struct led_classdev keyled = {
	.name = "key",
	.brightness_set = set_key,
	.default_trigger = "null",
};


#ifdef CONFIG_HAS_EARLYSUSPEND
void kb60_led_early_suspend(struct early_suspend *h)
{
        printk("%s:\n", __func__);
        //printk("%s: mpp_config_i_sink mpp9 ======================flag (%d)\n",__func__, keylight_flag);
	if(keylight_flag)
          {
	   if (!TimerCrl_flag) 
               hrtimer_cancel(&led_timer);
           TimerCrl_flag = 0; 
           disable_mpp9(0);
          }
}

void kb60_led_later_resume(struct early_suspend *h)
{
	int rc = 0;
        printk("%s:\n", __func__);
       	led_switch_mpp9 = mpp_get(NULL, "mpp9");
	if (!InCallScreen_flag)
	{
		rc = rpc_mpp_config_i_sink(led_switch_mpp9, PM_MPP__I_SINK__LEVEL_10mA, PM_MPP__I_SINK__SWITCH_ENA); 
		KeyBackLightLedOn_flag = F_KEYLED_ON;/*KYONCERA_NEW SPECIFICATION*/

		if (rc!=0) {
			printk(KERN_ERR "%s: mpp_config_i_sink mpp9 failed (%d)\n",__func__, rc);
			return;
		} 

		// [KB62_CR#145] WF  fix key backlight off when key backlight setting is on
		if (!KeepingOn_flag) {
			hrtimer_start(&led_timer,ktime_set(8,0), HRTIMER_MODE_REL);  // WF: 8s timer
		}
		keylight_flag = 1;
	}
}

void led_register_driver(void)
{
        #ifdef CONFIG_HAS_EARLYSUSPEND
	 kb60_led_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
        kb60_led_suspend.suspend = kb60_led_early_suspend;
        kb60_led_suspend.resume = kb60_led_later_resume;
        register_early_suspend(&kb60_led_suspend);
        #endif
}
#endif

static int led_suspend(struct platform_device *dev, pm_message_t state)
{
      int rc ; 
      //printk("%s: mpp_config_i_sink mpp9 ======================flag (%d)\n",__func__, keylight_flag);
      if(keylight_flag)
        {
          if (!TimerCrl_flag) 
             hrtimer_cancel(&led_timer);
	  rc = 0;
	  //printk("%s: mpp_config_i_sink mpp9 rc = (%d)\n",__func__, rc);	
	  rc = rpc_mpp_config_i_sink(led_switch_mpp9, PM_MPP__I_SINK__LEVEL_20mA, PM_MPP__I_SINK__SWITCH_DIS);
		KeyBackLightLedOn_flag = F_KEYLED_OFF;/*KYONCERA_NEW SPECIFICATION*/
	  if (rc!=0)
	     printk(KERN_ERR"%s: mpp_config_i_sink mpp9 failed (%d)\n",__func__, rc);
             TimerCrl_flag = 0;
             keylight_flag = 0 ;  
         }
        return 0;
}

static struct platform_driver msm_led_driver = {
	.suspend = led_suspend,
        .driver = {
		   .name = "msm-led",
		   .owner = THIS_MODULE,
		   },
};

static int __init cci_kb60_leds_init(void)
{
        int rc;
        led_classdev_register(NULL, &redled);
	led_classdev_register(NULL, &keyled);
	led_classdev_register(NULL, &greenled);
//suspend support
#ifdef CONFIG_HAS_EARLYSUSPEND
        led_register_driver();
#endif
        rc = platform_driver_register(&msm_led_driver);
        if (rc < 0) {
	     printk(KERN_ERR "%s: platform_driver_register failed for "
	      	"led driver. rc = %d\n", __func__, rc);
	     return rc;
	   }
        hrtimer_init(&led_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	led_timer.function = (void*)kb60_android_key_led_turn_off;
        INIT_WORK(&LEDMon_work, KeypadLed_shutdown);      
	return 0;
}

static void __exit cci_kb60_leds_exit(void)
{
	led_classdev_unregister(&redled);
	led_classdev_unregister(&keyled);
	led_classdev_unregister(&greenled);
        platform_driver_unregister(&msm_led_driver);
}

module_init(cci_kb60_leds_init);
module_exit(cci_kb60_leds_exit);

MODULE_DESCRIPTION("LEDs driver for Qualcomm MSM7225A on KB60");
MODULE_AUTHOR("Wayne Yu");
MODULE_LICENSE("GPL v2");

