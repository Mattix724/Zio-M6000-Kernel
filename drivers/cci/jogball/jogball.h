#ifndef CCI_JOGBALL_H
#define CCI_JOGBALL_H
/*******************************************
 * jogball.h
 * Copyright (C) 2009 CCI, Inc.
 * Author: Johnny Lee
 * Created on: 29-July.
 * Description:
 * For CCI KB60 Track Ball Driver Header File.
 *******************************************/


/*******************************************************************************
 * DEFINITIONS
 *******************************************************************************/
/*GPIO pin definitions*/
#define JOGBALL_KEY_GPIO_ENTER 50
#define JOGBALL_KEY_GPIO_UP    29
#define JOGBALL_KEY_GPIO_DOWN  28
#define JOGBALL_KEY_GPIO_LEFT  27  //36
#define JOGBALL_KEY_GPIO_RIGHT 35
// for sprint
#define JOGBALL_KEY_GPIO_RIGHT_SPRINT 20

/*Parameter definitions*/
#define JOGBALL_DIRECTION_WAY 4
#define JOGBALL_KEY_NUM 5

/*Tuning time parameters*/
#define DIRECTION_KEY_SAMPLING_RATE 150*1000000L /*Direction key sampling rate (ms)*(ms_to_ns)*/

/*Weight of Jogball's 4-way*/
/*These parameters are used to modify sensitivity of each directions to fit user's custom*/
#define JB_WEIGHT_UP    1
#define JB_WEIGHT_DOWN  1
#define JB_WEIGHT_LEFT  1
#define JB_WEIGHT_RIGHT 1

#define JB_EVENT_THRESHOLD 3

/*Debug Message Flag*/
#define CCI_JOGBALL_DEBUG 0


/*******************************************************************************
 * MACROS
 *******************************************************************************/
#define IRQ_TO_MSM(n) (n-64)
#define MSM_TO_IRQ(n) (n+64)

#if (CCI_JOGBALL_DEBUG == 1)
    #define jb_printk(fmt, arg...) \
        printk(KERN_INFO fmt, ##arg)
#else
    #define jb_printk(fmt, arg...) \
        ({ if (0) printk(KERN_INFO fmt, ##arg); 0; })
#endif

/*******************************************************************************
 * STRUCTURES AND ENUMS
 *******************************************************************************/
struct jogball_keytype
{
    int gpio_num;
    char *name;
    int input_key;
};

enum jbkey_define
{
    JBKEY_ENTER = 0,
    JBKEY_UP    = 1,
    JBKEY_DOWN  = 2,
    JBKEY_LEFT  = 3,
    JBKEY_RIGHT = 4
};

#endif /*CCI_JOGBALL_H*/
